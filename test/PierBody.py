import sys, os
import cadquery as cq
sys.path.append(os.path.dirname(__file__))
from test_cq_param_tracker import CqParametricTracker

def run():
    tracker = CqParametricTracker(model_name="PierBody")
    tracker.start_tracking()

    context = globals().copy()
    context['Height'] = float(sys.argv[1])*1000.0 if len(sys.argv)>1 else 8000.0
    context['cq'] = cq
    
    script_path = os.path.join(os.path.dirname(__file__), "../cq_script/PierBody.py")
    with open(script_path, "r", encoding="utf-8") as f:
        exec(f.read(), context)
    
    result = context.get("result")
    if result is not None:
        cq.exporters.export(result, os.path.join(os.path.dirname(__file__), "../export_PierBody.brep"))
        
    tracker.stop_tracking()
    
    features = tracker.recipe.get("features", [])
    valid_features = [f["id"] for f in features if f["type"] in ["IfcSectionedSpine"]]
        
    tracker.recipe["products"] = [{
        "id": "Product_PierBody",
        "ifc_type": "IfcColumn",
        "predefined_type": "COLUMN",
        "name": "墩身",
        "representations": valid_features
    }]
    
    import json
    with open(os.path.join(os.path.dirname(__file__), "PierBody.json"), "w", encoding="utf-8") as f:
        f.write(json.dumps(tracker.recipe, indent=2, ensure_ascii=False))

if __name__ == "__main__":
    run()
