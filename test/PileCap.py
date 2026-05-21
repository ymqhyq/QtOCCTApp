import sys, os
import cadquery as cq
sys.path.append(os.path.dirname(__file__))
from test_cq_param_tracker import CqParametricTracker

def run():
    tracker = CqParametricTracker(model_name="PileCap")
    tracker.start_tracking()

    context = globals().copy()
    context['Length'] = float(sys.argv[1])*1000.0 if len(sys.argv)>1 else 7000.0
    context['Width'] = float(sys.argv[2])*1000.0 if len(sys.argv)>2 else 5000.0
    context['Height'] = float(sys.argv[3])*1000.0 if len(sys.argv)>3 else 2000.0
    context['Layers'] = int(sys.argv[4]) if len(sys.argv)>4 else 1
    context['cq'] = cq
    
    script_path = os.path.join(os.path.dirname(__file__), "../cq_script/PileCap.py")
    with open(script_path, "r", encoding="utf-8") as f:
        exec(f.read(), context)
    
    result = context.get("result")
    if result is not None:
        feature_ids = getattr(result, "_current_feature_ids", [])
        tracker.add_product(product_id="Product_PileCap", ifc_type="IfcFooting", predefined_type="PILE_CAP", name="承台", representations=feature_ids)
    
    tracker.stop_tracking()
    with open(os.path.join(os.path.dirname(__file__), "PileCap.json"), "w", encoding="utf-8") as f:
        f.write(tracker.export_json())

if __name__ == "__main__":
    run()
