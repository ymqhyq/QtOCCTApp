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

POOL_WORKER_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pool_worker.py")
FALLBACK_WORKER_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "worker.py")

# 工作进程池大小（建议 CPU 核心数的 50-75%）
POOL_SIZE = 8

class ScriptRequest(BaseModel):
    code: str
    args: Dict[str, Any] = {}
    model_type: Optional[str] = None
    format: Optional[str] = "step" # Default to 'step' but can be 'brep'


def _get_worker_env():
    """构建子进程环境变量"""
    env = os.environ.copy()
    python_dir = os.path.dirname(sys.executable)
    lib_bin = os.path.join(python_dir, "Library", "bin")
    if os.path.exists(lib_bin) and lib_bin.lower() not in env.get("PATH", "").lower():
        env["PATH"] = lib_bin + os.pathsep + env.get("PATH", "")
    return env

_WORKER_ENV = _get_worker_env()


class WorkerPool:
    """
    预热的常驻工作进程池。
    每个工作进程在启动时完成 cadquery 的导入（约1-2秒），
    之后通过 stdin 管道持续接收任务，无需重复导入。
    """
    
    def __init__(self, size: int = POOL_SIZE):
        self.size = size
        self.available: asyncio.Queue = asyncio.Queue()
        self._all_workers = []
        self._lock = asyncio.Lock()
    
    async def start(self):
        """启动所有工作进程并等待预热完成"""
        logger.info(f"正在预热 {self.size} 个工作进程（导入 cadquery）...")
        tasks = [self._spawn_worker(i) for i in range(self.size)]
        results = await asyncio.gather(*tasks, return_exceptions=True)
        
        ready_count = 0
        for i, result in enumerate(results):
            if isinstance(result, asyncio.subprocess.Process):
                self._all_workers.append(result)
                await self.available.put(result)
                ready_count += 1
            else:
                logger.warning(f"工作进程 {i} 启动失败: {result}")
        
        logger.info(f"工作进程池就绪: {ready_count}/{self.size} 个进程已预热")
    
    async def _spawn_worker(self, worker_id: int = 0):
        """启动一个工作进程并等待其发出 READY 信号"""
        proc = await asyncio.create_subprocess_exec(
            sys.executable, POOL_WORKER_SCRIPT,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            env=_WORKER_ENV
        )
        
        try:
            # 等待 READY 信号（最多等 30 秒）
            line = await asyncio.wait_for(proc.stdout.readline(), timeout=30.0)
            if b"READY" in line:
                logger.info(f"工作进程 #{worker_id} (PID {proc.pid}) 预热完成")
                return proc
            else:
                logger.warning(f"工作进程 #{worker_id} 返回异常信号: {line}")
                proc.kill()
                return None
        except asyncio.TimeoutError:
            logger.warning(f"工作进程 #{worker_id} 预热超时")
            proc.kill()
            return None
    
    async def execute(self, code_file: str, args_file: str, output_path: str) -> tuple[bool, dict]:
        """向空闲工作进程分发一个任务"""
        logger.info(f"等待空闲工作进程... (当前队列大小: {self.available.qsize()})")
        worker = await self.available.get()
        logger.info(f"获取工作进程 PID {worker.pid}")
        updated_args = {}
        
        try:
            # 检查进程是否还活着
            if worker.returncode is not None:
                raise RuntimeError("工作进程已退出")
            
            task = json.dumps({
                "code_file": code_file,
                "args_file": args_file,
                "output_path": output_path
            })
            worker.stdin.write((task + "\n").encode())
            await worker.stdin.drain()
            
            # 等待结果（最多 120 秒）
            line = await asyncio.wait_for(worker.stdout.readline(), timeout=120.0)
            try:
                result = line.decode('utf-8').strip()
            except UnicodeDecodeError:
                result = line.decode('gbk', errors='ignore').strip()
            
            if result == "OK":
                await self.available.put(worker)
                logger.info(f"工作进程 PID {worker.pid} 任务完成 (OK)")
                # 尝试读取导出的参数
                out_args_path = args_file + ".out"
                if os.path.exists(out_args_path):
                    try:
                        with open(out_args_path, "r", encoding="utf-8") as f:
                            updated_args = json.load(f)
                        os.remove(out_args_path)
                    except Exception as e:
                        logger.warning(f"读取导出的参数失败: {e}")
                return True, updated_args
            elif result == "ERR":
                await self.available.put(worker)
                logger.info(f"工作进程 PID {worker.pid} 任务失败 (ERR)")
                return False, {}
            else:
                # 异常输出，进程可能已损坏
                raise RuntimeError(f"工作进程异常输出: {result}")
                
        except Exception as e:
            logger.warning(f"工作进程 PID {worker.pid} 异常: {e}，正在替换...")
            try:
                worker.kill()
            except Exception:
                pass
            
            # 启动替换进程
            replacement = await self._spawn_worker()
            if replacement:
                await self.available.put(replacement)
            
            return False, {}
    
    async def shutdown(self):
        """关闭所有工作进程"""
        for worker in self._all_workers:
            try:
                if worker.returncode is None:
                    worker.stdin.write(b"EXIT\n")
                    await worker.stdin.drain()
                    await asyncio.wait_for(worker.wait(), timeout=5.0)
            except Exception:
                try:
                    worker.kill()
                except Exception:
                    pass


# 全局工作进程池
worker_pool = WorkerPool(POOL_SIZE)

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
    """服务启动时预热工作进程池"""
    await worker_pool.start()

@app.get("/api/v1/schemas")
async def get_schemas():
    """获取所有模型的 Schema 定义"""
    load_schemas()
    return MODELS_SCHEMA


@app.on_event("shutdown") 
async def shutdown_event():
    """服务关闭时清理工作进程"""
    await worker_pool.shutdown()


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
        
        with open(code_file, "w", encoding="utf-8") as f:
            f.write(code)
        with open(args_file, "w", encoding="utf-8") as f:
            json.dump(request.args, f)
        
        # 分发到预热的工作进程池（非阻塞）
        success, updated_args = await worker_pool.execute(code_file, args_file, output_path)
        
        # 使用更新后的参数（包含脚本计算出的结果）进行返回
        effective_args = request.args.copy()
        effective_args.update(updated_args)
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    finally:
        # 清理临时文件
        for f in [code_file, args_file]:
            if os.path.exists(f):
                os.remove(f)
        
    error_file = output_path + ".err"
    if os.path.exists(error_file):
        with open(error_file, "r", encoding="utf-8") as f:
            err_msg = f.read()
        raise HTTPException(status_code=400, detail=f"脚本错误:\n{err_msg}")

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
