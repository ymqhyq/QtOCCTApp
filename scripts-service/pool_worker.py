"""
常驻工作进程：启动时预导入 cadquery（耗时约1-2秒），
之后通过 stdin/stdout 管道持续接收多个建模任务，避免重复导入开销。
"""
import sys
import json
import traceback
import os

# ====== 预热阶段：一次性导入所有重量级依赖 ======
import cadquery as cq
from OCP.TopoDS import TopoDS_Shape
from OCP.BRepTools import BRepTools

def execute_task(code, args, output_path, args_file=None):
    """在当前进程中执行 CadQuery 脚本并导出 BREP"""
    local_vars = {"cq": cq}
    for k, v in args.items():
        if isinstance(v, str):
            v_stripped = v.strip()
            if v_stripped == "":
                continue
            try:
                v = float(v_stripped)
            except ValueError:
                pass
        local_vars[k] = v
    
    exec(code, local_vars, local_vars)
    
    # 提取脚本执行后的参数值（回传给前端）
    if args_file:
        updated_args = {}
        # 收集所有基础类型的本地变量，包括脚本计算出的 out 参数
        for k, v in local_vars.items():
            if not k.startswith('_') and k != 'cq' and k != 'result' and k != 'shape_to_export':
                if isinstance(v, (int, float, str, bool)):
                    updated_args[k] = v
        
        with open(args_file + ".out", "w", encoding="utf-8") as f:
             json.dump(updated_args, f, ensure_ascii=False)

    if "result" not in local_vars:
        raise KeyError("脚本没有输出包含 'result' 变量")
    
    result = local_vars["result"]
    ext = os.path.splitext(output_path)[1].upper().replace(".", "")
    if ext not in ["STEP", "IGES", "BREP", "STL"]:
        ext = "STEP" # Default
        
    if isinstance(result, cq.Assembly):
        if ext == "BREP":
            # Assemblies cannot be saved as BREP directly, convert to Compound
            cq.exporters.export(result.toCompound(), output_path, ext)
        else:
            result.save(output_path, ext)
    else:
        # Fallback for Workplane and other shapes
        cq.exporters.export(result, output_path, ext)

# 通知主进程：预热完毕，可以接收任务
print("READY", flush=True)

# ====== 任务循环：持续等待并处理任务 ======
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    if line == "EXIT":
        break
    
    try:
        task = json.loads(line)
        code_file = task["code_file"]
        args_file = task["args_file"]
        output_path = task["output_path"]
        
        try:
            with open(code_file, 'r', encoding='utf-8') as f:
                code = f.read()
        except UnicodeDecodeError:
            with open(code_file, 'r', encoding='gbk') as f:
                code = f.read()
        with open(args_file, 'r', encoding='utf-8') as f:
            args = json.load(f)
        
        execute_task(code, args, output_path, args_file)
        print("OK", flush=True)
        
    except Exception:
        # 写入错误文件供主进程读取
        err_path = task.get("output_path", "unknown") + ".err"
        with open(err_path, "w", encoding="utf-8") as f:
            f.write(traceback.format_exc())
        print("ERR", flush=True)
