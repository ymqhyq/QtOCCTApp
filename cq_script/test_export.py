import cadquery as cq
import os

def run_modeling_and_export(script_name, params, output_brep):
    """
    执行建模脚本并导出 BREP
    """
    print(f"--- 正在处理: {script_name} ---")
    
    # 1. 准备执行环境 (模拟全局变量)
    # CadQuery 脚本通常从 globals() 获取参数
    context = params.copy()
    context['cq'] = cq  # 确保脚本内可以使用 cq
    
    # 2. 读取并执行脚本
    # 修正路径处理：使用脚本所在目录
    script_dir = os.path.dirname(os.path.abspath(__file__))
    script_path = os.path.join(script_dir, script_name)
    
    if not os.path.exists(script_path):
        print(f"错误: 找不到脚本 {script_path}")
        return

    with open(script_path, 'r', encoding='utf-8') as f:
        code = f.read()
    
    try:
        # 在 context 命名空间中执行
        exec(code, context)
        
        # 3. 获取结果并导出
        if 'result' in context:
            result = context['result']
            # 确保输出路径也是相对于脚本目录或当前目录
            export_path = os.path.join(script_dir, output_brep)
            print(f"导出到: {export_path}")
            # 执行导出
            cq.exporters.export(result, export_path)
        else:
            print(f"警告: 脚本 {script_name} 执行后未发现 'result' 变量")
            
    except Exception as e:
        print(f"执行脚本时发生异常: {str(e)}")
    print("\n")

if __name__ == "__main__":
    # 定义测试任务
    test_tasks = [
        {
            "name": "承台 (PileCap)",
            "script": "PileCap.py",
            "params": {"Length": 8000.0, "Width": 6000.0, "Height": 2500.0, "Layers": 1},
            "output": "export_pile_cap.brep"
        },
        {
            "name": "桩基础 (Pile)",
            "script": "Pile.py",
            "params": {"Diameter": 1200.0, "Length": 20000.0, "Layout": "2x3", "HSpacing": 3000.0},
            "output": "export_piles.brep"
        },
        {
            "name": "墩身 (PierBody)",
            "script": "PierBody.py",
            "params": {"Height": 12000.0},
            "output": "export_pier_body.brep"
        }
    ]

    # 执行所有测试
    for task in test_tasks:
        run_modeling_and_export(task["script"], task["params"], task["output"])

    print("所有导出任务已完成。")
