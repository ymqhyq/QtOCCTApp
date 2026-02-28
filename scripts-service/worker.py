import sys
import json
import traceback
import os

def main():
    if len(sys.argv) < 4:
        sys.exit("Usage: python worker.py <code_file> <args_file> <output_path>")
        
    code_file = sys.argv[1]
    args_file = sys.argv[2]
    output_path = sys.argv[3]
    
    try:
        with open(code_file, 'r', encoding='utf-8') as f:
            code = f.read()
        with open(args_file, 'r', encoding='utf-8') as f:
            args = json.load(f)
            
        import cadquery as cq
        
        local_vars = {"cq": cq}
        for k, v in args.items():
            local_vars[k] = v
            
        exec(code, local_vars, local_vars)
        
        if "result" not in local_vars:
            raise KeyError("脚本没有输出包含 cq.Workplane 或 TopoDS_Shape 的 'result' 变量。")
            
        result = local_vars["result"]
        
        # 与旧版 run_cq.py 保持完全一致的导出逻辑
        # 统一提取底层 TopoDS_Shape 并用 BRepTools.Write_s 写出
        from OCP.TopoDS import TopoDS_Shape
        from OCP.BRepTools import BRepTools
        
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
        
        if shape_to_export is None or shape_to_export.IsNull():
            raise ValueError("导出的形状为空")
            
        BRepTools.Write_s(shape_to_export, output_path)
        
        # 输出材质信息（如脚本中定义了 material 变量）
        material = local_vars.get('material', None)
        if material:
            print(f"SUCCESS|{str(material).upper()}", flush=True)
        else:
            print("SUCCESS", flush=True)
            
    except Exception as e:
        error_file = output_path + ".err"
        with open(error_file, "w", encoding="utf-8") as f:
            f.write(traceback.format_exc())
        sys.exit(1)

if __name__ == "__main__":
    main()
