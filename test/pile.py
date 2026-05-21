import sys
import os
import json
import cadquery as cq

# 添加当前目录以便引入 tracker
sys.path.append(os.path.dirname(__file__))
from test_cq_param_tracker import CqParametricTracker

def run_pile_model():
    # 动态解析桩长参数 (假定传入的参数为米, 默认 8m)
    pile_length = 8000.0
    if len(sys.argv) > 1:
        try:
            pile_length = float(sys.argv[1]) * 1000.0
            print(f"[Info] 设置桩长参数为: {pile_length} mm")
        except ValueError:
            pass

    # 创建并激活拦截器 (Model Name 设定为 pile)
    tracker = CqParametricTracker(model_name="pile")
    tracker.start_tracking()

    # ---------------------------------------------------------
    # 仅建模桩基础，不包含承台与墩身
    # ---------------------------------------------------------
    print("[Info] 正在运行桩基础(Piles)建模代码...")
    pts = []
    hSpacing = 2500.0
    vSpacing = 3000.0
    for xi in [-hSpacing, 0, hSpacing]:
        for yi in [-vSpacing / 2.0, vSpacing / 2.0]:
            pts.append((xi, yi))
            
    piles_wp = (
        cq.Workplane("XY")
        .workplane(offset=-2000.0) # 假设桩顶起算标高
        .pushPoints(pts)
        .circle(500.0)
        .extrude(-pile_length)
    )
    piles_feature_ids = getattr(piles_wp, "_current_feature_ids", [])
    tracker.add_product(
        product_id="Product_Pile_01",
        ifc_type="IfcPile",
        predefined_type="BORED",
        name="基础灌注桩组",
        representations=piles_feature_ids
    )

    # ---------------------------------------------------------
    # 注销拦截器并导出 JSON
    # ---------------------------------------------------------
    tracker.stop_tracking()
    
    recipe_json = tracker.export_json()
    # 默认输出名为 pile.json，C++ 层会将其流转重命名为 pile_8.json
    output_json_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "pile.json"))
    
    with open(output_json_path, "w", encoding="utf-8") as f:
        f.write(recipe_json)
        
    print(f"[Success] Parametric CIPR JSON written successfully to:\n  {output_json_path}\n")

if __name__ == "__main__":
    run_pile_model()
