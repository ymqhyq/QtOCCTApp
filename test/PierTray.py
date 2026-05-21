import sys, os
import cadquery as cq
sys.path.append(os.path.dirname(__file__))
from test_cq_param_tracker import CqParametricTracker

def run():
    tracker = CqParametricTracker(model_name="PierTray")
    tracker.start_tracking()

    context = globals().copy()
    context['Height'] = float(sys.argv[1])*1000.0 if len(sys.argv)>1 else 3000.0
    context['cq'] = cq
    
    script_path = os.path.join(os.path.dirname(__file__), "../cq_script/PierTray.py")
    with open(script_path, "r", encoding="utf-8") as f:
        exec(f.read(), context)
    
    result = context.get("result")
    if result is not None:
        cq.exporters.export(result, os.path.join(os.path.dirname(__file__), "../export_PierTray.brep"))
        
    tracker.stop_tracking()
    
    # 手动绑定正确的 feature 到 Product_PierTray，并移除幽灵组
    features = tracker.recipe.get("features", [])
    valid_features = [f["id"] for f in features if f["type"] == "IfcBooleanResult"] # 托盘是 cut 出来的
    if not valid_features:
        valid_features = [f["id"] for f in features if f["type"] in ["IfcSectionedSpine", "IfcExtrudedAreaSolid"]]
        
    # 清理所有的 Products，只保留真正的托盘
    tracker.recipe["products"] = [{
        "id": "Product_PierTray",
        "ifc_type": "IfcBuildingElementProxy",
        "predefined_type": "COMPLEX",
        "name": "托盘",
        "representations": [valid_features[0]] if valid_features else []
    }]
    
    import json
    with open(os.path.join(os.path.dirname(__file__), "PierTray.json"), "w", encoding="utf-8") as f:
        f.write(json.dumps(tracker.recipe, indent=2, ensure_ascii=False))

if __name__ == "__main__":
    run()
