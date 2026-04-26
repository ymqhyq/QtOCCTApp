"""
OCCT 建模微服务 - 非阻塞并发版
使用预热常驻工作进程池，消除重复 import cadquery 的开销。
"""
import os
import sys
import uuid
import json
import asyncio
import logging
import struct
import hashlib
import base64
import urllib.request
import urllib.error
from fastapi import FastAPI, HTTPException, Response
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Dict, Any, Optional

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("ModelingService")

app = FastAPI(title="OCCT Modeling Service")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

WORKSPACE = os.path.abspath(os.path.join(os.path.dirname(__file__), "workspace"))
WEB_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "web"))
os.makedirs(WORKSPACE, exist_ok=True)
os.makedirs(WEB_DIR, exist_ok=True)

class ScriptRequest(BaseModel):
    code: str
    args: Dict[str, Any] = {}
    model_type: Optional[str] = None
    format: Optional[str] = "step"

import traceback
import cadquery as cq
from OCP.TopoDS import TopoDS_Shape
from OCP.BRepTools import BRepTools

# 全局锁：保证单进程内的 CadQuery（特别是 multimethod 分发字典）执行严格串行，防止多线程重入报错
cq_lock = asyncio.Lock()


def execute_task(code, args, output_path):
    """在当前进程中执行 CadQuery 脚本并导出"""
    local_vars = {"cq": cq}
    for k, v in args.items():
        if isinstance(v, str):
            v_stripped = v.strip()
            if v_stripped == "": continue
            try: v = float(v_stripped)
            except ValueError: pass
        local_vars[k] = v
    
    exec(code, local_vars, local_vars)
    
    updated_args = {}
    for k, v in local_vars.items():
        if not k.startswith('_') and k != 'cq' and k != 'result' and k != 'shape_to_export':
            if isinstance(v, (int, float, str, bool)):
                updated_args[k] = v
                
    if "result" not in local_vars:
        raise KeyError("脚本没有输出包含 'result' 变量")
        
    result = local_vars["result"]
    ext = os.path.splitext(output_path)[1].upper().replace(".", "")
    if ext not in ["STEP", "IGES", "BREP", "STL"]:
        ext = "STEP"
        
    if isinstance(result, cq.Assembly):
        if ext == "BREP":
            cq.exporters.export(result.toCompound(), output_path, ext)
        else:
            result.save(output_path, ext)
    else:
        cq.exporters.export(result, output_path, ext)
        
    return updated_args

def compute_cache_key(request: ScriptRequest) -> str:
    key_dict = {
        "model_type": request.model_type,
        "args": request.args,
        "code": request.code,
        "format": request.format
    }
    key_str = json.dumps(key_dict, sort_keys=True)
    return hashlib.sha256(key_str.encode('utf-8')).hexdigest()

def dapr_get_state(key: str):
    dapr_port = os.getenv("DAPR_HTTP_PORT", "3500")
    url = f"http://127.0.0.1:{dapr_port}/v1.0/state/statestore/{key}"
    try:
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req) as response:
            if response.status == 200:
                data = response.read()
                if data:
                    return json.loads(data)
    except urllib.error.HTTPError as e:
        if e.code not in (204, 404):
            logger.error(f"Dapr get state HTTP Error: {e}")
    except Exception as e:
        logger.error(f"Dapr get state exception: {e}")
    return None

def dapr_save_state(key: str, value: dict):
    dapr_port = os.getenv("DAPR_HTTP_PORT", "3500")
    url = f"http://127.0.0.1:{dapr_port}/v1.0/state/statestore"
    payload = [{"key": key, "value": value}]
    data = json.dumps(payload).encode('utf-8')
    try:
        req = urllib.request.Request(url, data=data, headers={'Content-Type': 'application/json'}, method='POST')
        with urllib.request.urlopen(req) as response:
            pass
    except Exception as e:
        logger.error(f"Dapr save state exception: {e}")

# WorkerPool 已被移除，改为直接利用 FastAPI 并发和 Dapr 多实例部署

# 加载 Schema 定义 (如果有 YAML 库用 YAML，这里暂时先预留加载逻辑)
MODELS_SCHEMA = {}
SCHEMA_FILE = os.path.join(os.path.dirname(__file__), "models_schema.yaml")

def load_schemas():
    global MODELS_SCHEMA
    if not os.path.exists(SCHEMA_FILE):
        return
    try:
        # 如果没有 yaml 库，我们尝试探测它
        import yaml
        with open(SCHEMA_FILE, "r", encoding="utf-8") as f:
            MODELS_SCHEMA = yaml.safe_load(f)
    except ImportError:
        logger.warning("未找到 PyYAML 库，Schema 渲染可能受限。建议安装: pip install PyYAML")
        # 极简解析器，仅支持简单键值对 (fallback)
        pass
    except Exception as e:
        logger.error(f"加载 Schema 失败: {e}")

load_schemas()


@app.on_event("startup")
async def startup_event():
    """服务启动事件"""
    pass

@app.get("/api/v1/schemas")
async def get_schemas():
    """获取所有模型的 Schema 定义"""
    load_schemas()
    return MODELS_SCHEMA

@app.on_event("shutdown") 
async def shutdown_event():
    """服务关闭事件"""
    pass


@app.post("/api/v1/model/generate")
async def generate_model(request: ScriptRequest):
    load_schemas() # 调试期间确保 Schema 始终最新
    task_id = str(uuid.uuid4())
    
    # 根据请求指定格式（默认 step）
    ext = (request.format or "step").lower()
    if ext not in ["step", "brep", "iges", "stl"]:
        ext = "step"
        
    output_path = os.path.join(WORKSPACE, f"{task_id}.{ext}")
    logger.info(f"生成任务 {task_id}: 格式={ext}, 模型类型={request.model_type}")
    
    code_file = os.path.join(WORKSPACE, f"{task_id}_code.py")
    args_file = os.path.join(WORKSPACE, f"{task_id}_args.json")
    
    # 尝试从 Dapr 缓存中获取 (临时禁用以强制重新生成，但保留 cache_key 定义供后续使用)
    cache_key = compute_cache_key(request)
    # try:
    #     cached_data = await asyncio.to_thread(dapr_get_state, cache_key)
    #     if cached_data and "brep_b64" in cached_data:
    #         logger.info(f"命中 Dapr 缓存: {cache_key}")
    #         brep_bytes = base64.b64decode(cached_data["brep_b64"])
    #         metadata = cached_data["metadata"]
    #         json_bytes = json.dumps(metadata, ensure_ascii=False).encode("utf-8")
    #         header = struct.pack("<I", len(json_bytes))
    #         full_package = header + json_bytes + brep_bytes
    #         return Response(
    #             content=full_package,
    #             media_type="application/octet-stream",
    #             headers={"Content-Disposition": f"attachment; filename={task_id}.jhb"}
    #         )
    # except Exception as e:
    #     logger.error(f"读取 Dapr 缓存失败: {e}")
        
    try:
        code = request.code
        # 如果代码为空但提供了模型类型，则尝试读取同名脚本文件
        if not code and request.model_type:
            script_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "cq_script", f"{request.model_type}.py"))
            if os.path.exists(script_path):
                try:
                    with open(script_path, "r", encoding="utf-8") as sf:
                        code = sf.read()
                except UnicodeDecodeError:
                    with open(script_path, "r", encoding="gbk") as sf:
                        code = sf.read()
            else:
                raise HTTPException(status_code=400, detail=f"未找到脚本: {script_path}")
        
        # 直接利用 FastAPI 线程池执行，不依赖 WorkerPool
        # 增加 asyncio.Lock() 以防单个实例内出现多线程重入导致的 CadQuery 内部字典迭代错误
        async with cq_lock:
            updated_args = await asyncio.to_thread(execute_task, code, request.args, output_path)
        
        effective_args = request.args.copy()
        effective_args.update(updated_args)
    except Exception as e:
        err_msg = traceback.format_exc()
        logger.error(f"脚本执行失败: {err_msg}")
        raise HTTPException(status_code=500, detail=f"脚本错误:\n{err_msg}")

    if not os.path.exists(output_path):
        raise HTTPException(status_code=500, detail="脚本执行结束但未生成任何输出文件。")

    # JHB (JSON-Header + Binary-Body) 封装
    # 构造元数据
    raw_schema = MODELS_SCHEMA.get(request.model_type, {}) if request.model_type else {}
    ordered_schema = {}
    if raw_schema:
        # 提取构件显示名称
        ordered_schema["name"] = raw_schema.get("name", request.model_type)
        # 将字段字典转换为有序列表处理
        fields_list = []
        for key, info in raw_schema.items():
            if key == "name" or not isinstance(info, dict):
                continue
            field_data = info.copy()
            field_data["key"] = key
            fields_list.append(field_data)
        ordered_schema["fields"] = fields_list

    metadata = {
        "args": effective_args,
        "modelType": request.model_type,
        "name": ordered_schema.get("name", request.model_type),
        "schema": ordered_schema
    }
    
    try:
        json_bytes = json.dumps(metadata, ensure_ascii=False).encode("utf-8")
        
        with open(output_path, "rb") as f:
            brep_bytes = f.read()
            
        # 格式: [4字节长度 L][L字节 JSON][原始 BREP]
        # 使用小端序 (Little-endian) 以匹配 Windows/Qt 环境
        header = struct.pack("<I", len(json_bytes))
        full_package = header + json_bytes + brep_bytes
        
        # 存入 Dapr 缓存
        try:
            cache_val = {
                "metadata": metadata,
                "brep_b64": base64.b64encode(brep_bytes).decode('utf-8')
            }
            await asyncio.to_thread(dapr_save_state, cache_key, cache_val)
            logger.info(f"模型结果已存入 Dapr 缓存: {cache_key}")
        except Exception as e:
            logger.error(f"保存 Dapr 缓存时发生错误: {e}")
        
        return Response(
            content=full_package,
            media_type="application/octet-stream",
            headers={"Content-Disposition": f"attachment; filename={task_id}.jhb"}
        )
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"封装 JHB 失败: {e}")


@app.get("/api/v1/model/download/{task_id}")
async def download_model(task_id: str, ext: str = "step"):
    file_path = os.path.join(WORKSPACE, f"{task_id}.{ext}")
    if os.path.exists(file_path):
        from fastapi.responses import FileResponse
        return FileResponse(path=file_path, filename=f"{task_id}.{ext}", media_type="application/octet-stream")
    raise HTTPException(status_code=404, detail="文件未找到")


# Mount UI after other routes so it doesn't mask API routes
app.mount("/", StaticFiles(directory=WEB_DIR, html=True), name="web")

if __name__ == "__main__":
    import uvicorn 
    uvicorn.run(app, host="127.0.0.1", port=8000)
