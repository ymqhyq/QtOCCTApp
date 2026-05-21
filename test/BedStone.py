import sys, os
import cadquery as cq
sys.path.append(os.path.dirname(__file__))
from test_cq_param_tracker import CqParametricTracker

def run():
    tracker = CqParametricTracker(model_name="BedStone")
    tracker.start_tracking()

    context = globals().copy()
    context['cq'] = cq
    
    script_path = os.path.join(os.path.dirname(__file__), "../cq_script/BedStone.py")
    with open(script_path, "r", encoding="utf-8") as f:
        exec(f.read(), context)
    
    result = context.get("result")
    if result is not None:
        feature_ids = getattr(result, "_current_feature_ids", [])
        tracker.add_product(product_id="Product_BedStone", ifc_type="IfcBuildingElementProxy", predefined_type="COMPLEX", name="垫石", representations=feature_ids)
    
    tracker.stop_tracking()
    with open(os.path.join(os.path.dirname(__file__), "BedStone.json"), "w", encoding="utf-8") as f:
        f.write(tracker.export_json())

if __name__ == "__main__":
    run()
