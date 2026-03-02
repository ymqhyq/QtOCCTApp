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

def execute_task(code, args, output_path):
    """在当前进程中执行 CadQuery 脚本并导出 BREP"""
    local_vars = {"cq": cq}
    for k, v in args.items():
        local_vars[k] = v
    
    exec(code, local_vars, local_vars)
    
    if "result" not in local_vars:
        raise KeyError("脚本没有输出包含 'result' 变量")
    
    result = local_vars["result"]
    
    # 与旧版 run_cq.py 一致的导出逻辑
    shape_to_export = None
    if isinstance(result, cq.Workplane):
        shape_to_export = result.val().wrapped
    elif isinstance(result, cq.Assembly):
        shape_to_export = result.toCompound().wrapped
    elif isinstance(result, cq.Shape):
        shape_to_export = result.wrapped
    elif hasattr(result, 'wrapped'):
        shape_to_export = result.wrapped
    elif isinstance(result, TopoDS_Shape):
        shape_to_export = result
    else:
        raise TypeError(f"不支持的结果类型: {type(result)}")
    
    if shape_to_export is None:
        raise ValueError("导出的形状为空")
        
    # 确保是 TopoDS_Shape 类型
    if not isinstance(shape_to_export, TopoDS_Shape):
         raise TypeError(f"导出失败: 期望 TopoDS_Shape, 但得到 {type(shape_to_export)}。请检查脚本 result 是否为有效几何体。")

    if shape_to_export.IsNull():
        raise ValueError("导出的形状库对象为空 (IsNull)")
    
    BRepTools.Write_s(shape_to_export, output_path)

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
        
        with open(code_file, 'r', encoding='utf-8') as f:
            code = f.read()
        with open(args_file, 'r', encoding='utf-8') as f:
            args = json.load(f)
        
        execute_task(code, args, output_path)
        print("OK", flush=True)
        
    except Exception:
        # 写入错误文件供主进程读取
        err_path = task.get("output_path", "unknown") + ".err"
        with open(err_path, "w", encoding="utf-8") as f:
            f.write(traceback.format_exc())
        print("ERR", flush=True)
