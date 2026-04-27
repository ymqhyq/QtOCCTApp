print("DEBUG_START: Script initiated")
import sys
print(f"DEBUG_INFO: Python Version: {sys.version}")

try:
    print("DEBUG_STEP: Importing os...")
    import os
    print("DEBUG_STEP: Importing cadquery...")
    import cadquery as cq
    print("DEBUG_SUCCESS: All imports done")

    # 模拟环境
    sys.path.append(os.path.join(os.path.dirname(__file__), "..", "cq_script"))
    
    global_vars = {
        "cq": cq,
        "Diameter": 1000.0,
        "Length": 15000.0,
        "Layout": "2x3",
        "HSpacing": 2500.0,
        "VSpacing": 3000.0
    }
    
    script_path = os.path.join(os.path.dirname(__file__), "..", "cq_script", "SinglePile.py")
    print(f"DEBUG_STEP: Loading script from {script_path}")
    
    with open(script_path, "r", encoding="utf-8") as f:
        code = f.read()
    
    print("DEBUG_STEP: Executing script...")
    exec(code, global_vars, global_vars)
    
    if "result" in global_vars:
        print(f"DEBUG_RESULT: Success! Result type: {type(global_vars['result'])}")
    else:
        print("DEBUG_RESULT: Error: 'result' not found")

except Exception as e:
    import traceback
    print("DEBUG_EXCEPTION: Error caught!")
    traceback.print_exc()
