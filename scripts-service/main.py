import os
import sys
import uuid
import traceback
import subprocess
import json
from fastapi import FastAPI, HTTPException, BackgroundTasks
from fastapi.responses import FileResponse
from pydantic import BaseModel
from typing import Dict, Any

app = FastAPI(title="OCCT Modeling Service")

WORKSPACE = os.path.abspath(os.path.join(os.path.dirname(__file__), "workspace"))
os.makedirs(WORKSPACE, exist_ok=True)

class ScriptRequest(BaseModel):
    code: str
    args: Dict[str, Any] = {}

def execute_cq_script(code: str, args: Dict[str, Any], output_path: str):
    """" 
    在隔离的外部进程中执行 CadQuery 脚本并导出结果，防止底层共享内存状态引发服务端崩溃。
    """
    worker_script = os.path.join(os.path.dirname(__file__), "worker.py")
    task_id = os.path.basename(output_path).replace('.brep', '')
    
    code_file = os.path.join(WORKSPACE, f"{task_id}_code.py")
    args_file = os.path.join(WORKSPACE, f"{task_id}_args.json")
    
    with open(code_file, "w", encoding="utf-8") as f:
        f.write(code)
    with open(args_file, "w", encoding="utf-8") as f:
        json.dump(args, f)
        
    env = os.environ.copy()
    python_dir = os.path.dirname(sys.executable)
    lib_bin = os.path.join(python_dir, "Library", "bin")
    if os.path.exists(lib_bin) and lib_bin.lower() not in env.get("PATH", "").lower():
        env["PATH"] = lib_bin + os.pathsep + env.get("PATH", "")

    try:
        result = subprocess.run(
            [sys.executable, worker_script, code_file, args_file, output_path],
            capture_output=True,
            text=True,
            check=False,
            env=env
        )
        if result.returncode != 0:
            error_file = output_path + ".err"
            if not os.path.exists(error_file):
                with open(error_file, "w", encoding="utf-8") as f:
                    f.write(f"内部执行进程异常退出 (退出码 {result.returncode}):\n{result.stderr}")
    finally:
        if os.path.exists(code_file):
            os.remove(code_file)
        if os.path.exists(args_file):
            os.remove(args_file)

@app.post("/api/v1/model/generate")
async def generate_model(request: ScriptRequest, background_tasks: BackgroundTasks):
    task_id = str(uuid.uuid4())
    output_path = os.path.join(WORKSPACE, f"{task_id}.brep")
    
    # 在这个简单版本中我们暂时以同步方式执行脚本。
    # 对于长时间运行的重型模型生成任务，未来可改用 background_tasks 或 Celery 避免阻塞工作进程。
    
    try:
        execute_cq_script(request.code, request.args, output_path)
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
        
    error_file = output_path + ".err"
    if os.path.exists(error_file):
        with open(error_file, "r", encoding="utf-8") as f:
            err_msg = f.read()
        raise HTTPException(status_code=400, detail=f"脚本错误:\n{err_msg}")

    if not os.path.exists(output_path):
        raise HTTPException(status_code=500, detail="脚本执行结束但未生成任何输出文件。")

    # 直接返回 BREP 文件二进制内容，而不是下载链接的 JSON
    # 这样 C++ 客户端的 reply->readAll() 拿到的就是真正的 BREP 数据
    return FileResponse(path=output_path, filename=f"{task_id}.brep", media_type="application/octet-stream")


@app.get("/api/v1/model/download/{task_id}")
async def download_model(task_id: str):
    file_path = os.path.join(WORKSPACE, f"{task_id}.brep")
    if os.path.exists(file_path):
        return FileResponse(path=file_path, filename=f"{task_id}.brep", media_type="application/octet-stream")
    raise HTTPException(status_code=404, detail="文件未找到")

if __name__ == "__main__":
    import uvicorn 
    uvicorn.run(app, host="127.0.0.1", port=8000)
