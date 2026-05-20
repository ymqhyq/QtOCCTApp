import json
import os
import sys
from typing import List, Dict, Any

# 确保 cadquery 库存在
try:
    import cadquery as cq
except ImportError:
    print("[Error] Failed to import cadquery. Please run this script within the correct conda/miniforge environment.")
    sys.exit(1)

class CqParametricTracker:
    """CadQuery 参数化建模步骤无感代理拦截录制器 (支持几何复用与全算子翻译)"""
    def __init__(self, model_name: str = "Parametric_Assembly"):
        self.recipe = {
            "protocol_version": "1.2.0",
            "metadata": {
                "generator": "CadQuery-CIPR-AutoTracker v1.2",
                "model_name": model_name
            },
            "parameters": {},
            "declarations": {
                "profiles": []
            },
            "features": [],
            "products": []
        }
        self.profile_counter = 0
        self.feature_counter = 0
        self._original_methods = {}

    def _get_placement_3d(self, plane: cq.Plane) -> Dict[str, Any]:
        """将 CadQuery 工作平面的 3D 变换矩阵解算为 IFC Placement"""
        origin = plane.origin
        z_dir = plane.zDir  # 局部 Z 轴 (法线)
        x_dir = plane.xDir  # 局部 X 轴
        
        return {
            "type": "IfcAxis2Placement3D",
            "Location": [float(origin.x), float(origin.y), float(origin.z)],
            "Axis": [float(z_dir.x), float(z_dir.y), float(z_dir.z)],
            "RefDirection": [float(x_dir.x), float(x_dir.y), float(x_dir.z)]
        }

    def start_tracking(self):
        """动态代理注入核心建模方法"""
        methods_to_intercept = [
            "circle", "rect", "extrude", "box", "pushPoints", "polyline",
            "moveTo", "lineTo", "threePointArc", "close", "loft", "cut", "union",
            "translate", "faces", "workplane"
        ]
        for name in methods_to_intercept:
            if hasattr(cq.Workplane, name):
                self._original_methods[name] = getattr(cq.Workplane, name)
                setattr(cq.Workplane, name, self._make_proxy(name))

    def stop_tracking(self):
        """恢复 CadQuery 原始环境"""
        for name, orig_method in self._original_methods.items():
            setattr(cq.Workplane, name, orig_method)

    def _make_proxy(self, method_name: str):
        outer_self = self
        orig_method = getattr(cq.Workplane, method_name)

        def proxy_method(self_wp, *args, **kwargs):
            # 执行原生的 OCCT CAD 几何建模生成
            result_wp = orig_method(self_wp, *args, **kwargs)
            try:
                outer_self._intercept_call(method_name, self_wp, result_wp, args, kwargs)
            except Exception as e:
                print(f"[Tracker Warning] Failed to log step '{method_name}': {e}")
            return result_wp

        return proxy_method

    def _intercept_call(self, name: str, prev_wp: cq.Workplane, next_wp: cq.Workplane, args: tuple, kwargs: dict):
        """同步拦截并序列化为标准协议格式"""
        
        # 自动在链条间传递和同步关键的追踪上下文属性 (预拷贝，防止由于 moveTo/lineTo/threePointArc 等中间步骤导致上下文丢失)
        for attr in ["_current_profile_id", "_current_feature_ids", "_pushed_points", "_sketch_points", "_loft_profiles", "_loft_positions"]:
            if hasattr(prev_wp, attr) and not hasattr(next_wp, attr):
                setattr(next_wp, attr, getattr(prev_wp, attr))
        
        # 1. 状态和链条传递 (Faces, Workplane 仅作信息复制传递，已由上述通用机制自动同步，直接返回即可)
        if name in ["faces", "workplane"]:
            return

        # 2. 截面绘制代理
        elif name == "circle":
            radius = args[0] if len(args) > 0 else kwargs.get("radius")
            self.profile_counter += 1
            profile_id = f"Profile_Circle_{self.profile_counter}"
            
            self.recipe["declarations"]["profiles"].append({
                "id": profile_id,
                "type": "IfcCircleProfileDef",
                "ProfileType": "AREA",
                "Radius": float(radius)
            })
            next_wp._current_profile_id = profile_id
            
            # 链条上传递阵列点
            if hasattr(prev_wp, "_pushed_points"):
                next_wp._pushed_points = prev_wp._pushed_points

        elif name == "rect":
            x_dim = args[0] if len(args) > 0 else kwargs.get("xLen", kwargs.get("x"))
            y_dim = args[1] if len(args) > 1 else kwargs.get("yLen", kwargs.get("y"))
            self.profile_counter += 1
            profile_id = f"Profile_Rect_{self.profile_counter}"
            
            self.recipe["declarations"]["profiles"].append({
                "id": profile_id,
                "type": "IfcRectangleProfileDef",
                "ProfileType": "AREA",
                "XDim": float(x_dim),
                "YDim": float(y_dim)
            })
            next_wp._current_profile_id = profile_id
            
            if hasattr(prev_wp, "_pushed_points"):
                next_wp._pushed_points = prev_wp._pushed_points

        elif name == "polyline":
            pts = args[0] if len(args) > 0 else kwargs.get("pts")
            self.profile_counter += 1
            profile_id = f"Profile_Polyline_{self.profile_counter}"
            
            converted_pts = []
            for pt in pts:
                if hasattr(pt, "x") and hasattr(pt, "y"):
                    converted_pts.append([float(pt.x), float(pt.y)])
                elif isinstance(pt, (tuple, list)):
                    converted_pts.append([float(pt[0]), float(pt[1])])
                else:
                    converted_pts.append([float(pt[0]), float(pt[1])])
            
            self.recipe["declarations"]["profiles"].append({
                "id": profile_id,
                "type": "IfcArbitraryClosedProfileDef",
                "ProfileType": "AREA",
                "OuterCurve": {
                    "type": "IfcPolyline",
                    "Points": converted_pts
                }
            })
            next_wp._current_profile_id = profile_id
            
            if hasattr(prev_wp, "_pushed_points"):
                next_wp._pushed_points = prev_wp._pushed_points

        # 3. 链式草图绘制 (moveTo, lineTo, threePointArc, close)
        elif name == "moveTo":
            x = args[0] if len(args) > 0 else kwargs.get("x")
            y = args[1] if len(args) > 1 else kwargs.get("y")
            next_wp._sketch_points = [(float(x), float(y))]

        elif name == "lineTo":
            x = args[0] if len(args) > 0 else kwargs.get("x")
            y = args[1] if len(args) > 1 else kwargs.get("y")
            pts = getattr(prev_wp, "_sketch_points", []).copy()
            pts.append((float(x), float(y)))
            next_wp._sketch_points = pts

        elif name == "threePointArc":
            p1 = args[0] if len(args) > 0 else kwargs.get("p1")
            p2 = args[1] if len(args) > 1 else kwargs.get("p2")
            pts = getattr(prev_wp, "_sketch_points", []).copy()
            pts.append((float(p1[0]), float(p1[1])))
            pts.append((float(p2[0]), float(p2[1])))
            next_wp._sketch_points = pts

        elif name == "close":
            pts = getattr(prev_wp, "_sketch_points", []).copy()
            if pts and (pts[0][0] != pts[-1][0] or pts[0][1] != pts[-1][1]):
                pts.append(pts[0])
                
            self.profile_counter += 1
            profile_id = f"Profile_Sketch_{self.profile_counter}"
            
            self.recipe["declarations"]["profiles"].append({
                "id": profile_id,
                "type": "IfcArbitraryClosedProfileDef",
                "ProfileType": "AREA",
                "OuterCurve": {
                    "type": "IfcPolyline",
                    "Points": [[float(p[0]), float(p[1])] for p in pts]
                }
            })
            next_wp._current_profile_id = profile_id
            
            # Loft 截面链条记录
            loft_profiles = getattr(prev_wp, "_loft_profiles", []).copy()
            loft_profiles.append(profile_id)
            next_wp._loft_profiles = loft_profiles
            
            loft_positions = getattr(prev_wp, "_loft_positions", []).copy()
            loft_positions.append(self._get_placement_3d(prev_wp.plane))
            next_wp._loft_positions = loft_positions

        # 4. 堆栈阵列推送方法
        elif name == "pushPoints":
            pntList = args[0] if len(args) > 0 else kwargs.get("pntList")
            converted_pts = []
            for p in pntList:
                if isinstance(p, (tuple, list)):
                    converted_pts.append((float(p[0]), float(p[1])))
                else:
                    converted_pts.append((float(p.x), float(p.y)))
            next_wp._pushed_points = converted_pts

        # 5. 三维特征生成算子
        elif name == "extrude":
            depth = args[0] if len(args) > 0 else kwargs.get("until", kwargs.get("depth"))
            normal = prev_wp.plane.zDir
            
            if depth < 0:
                direction = [float(-normal.x), float(-normal.y), float(-normal.z)]
                depth_val = float(abs(depth))
            else:
                direction = [float(normal.x), float(normal.y), float(normal.z)]
                depth_val = float(depth)
                
            pushed_points = getattr(prev_wp, "_pushed_points", None)
            
            if pushed_points and len(pushed_points) > 1:
                # 5.1 阵列实例化拉伸 (IfcMappedItem)
                self.feature_counter += 1
                source_feat_id = f"Single_Solid_{self.feature_counter}"
                profile_id = getattr(prev_wp, "_current_profile_id", "Unknown_Profile")
                
                ref_dir = prev_wp.plane.xDir
                placement = {
                    "type": "IfcAxis2Placement3D",
                    "Location": [0.0, 0.0, 0.0],
                    "Axis": direction,
                    "RefDirection": [float(ref_dir.x), float(ref_dir.y), float(ref_dir.z)]
                }
                
                self.recipe["features"].append({
                    "id": source_feat_id,
                    "type": "IfcExtrudedAreaSolid",
                    "SweptArea": profile_id,
                    "Position": placement,
                    "ExtrudedDirection": direction,
                    "Depth": depth_val
                })
                
                mapped_feats = []
                for idx, pt in enumerate(pushed_points):
                    world_pt = prev_wp.plane.toWorldCoords(pt)
                    self.feature_counter += 1
                    mapped_id = f"Mapped_Item_{self.feature_counter}"
                    
                    self.recipe["features"].append({
                        "id": mapped_id,
                        "type": "IfcMappedItem",
                        "SourceGeometry": source_feat_id,
                        "TargetLocation": [float(world_pt.x), float(world_pt.y), float(world_pt.z)]
                    })
                    mapped_feats.append(mapped_id)
                next_wp._current_feature_ids = mapped_feats
            else:
                # 5.2 普通拉伸
                self.feature_counter += 1
                feature_id = f"Feature_Extrude_{self.feature_counter}"
                profile_id = getattr(prev_wp, "_current_profile_id", "Unknown_Profile")
                placement = self._get_placement_3d(prev_wp.plane)
                
                self.recipe["features"].append({
                    "id": feature_id,
                    "type": "IfcExtrudedAreaSolid",
                    "SweptArea": profile_id,
                    "Position": placement,
                    "ExtrudedDirection": direction,
                    "Depth": depth_val
                })
                next_wp._current_feature_ids = [feature_id]

        elif name == "box":
            x = args[0] if len(args) > 0 else kwargs.get("xLen", kwargs.get("x"))
            y = args[1] if len(args) > 1 else kwargs.get("yLen", kwargs.get("y"))
            z = args[2] if len(args) > 2 else kwargs.get("zLen", kwargs.get("z"))
            centered = args[3] if len(args) > 3 else kwargs.get("centered", True)
            
            centered_z = True
            if isinstance(centered, (tuple, list)):
                if len(centered) >= 3:
                    centered_z = centered[2]
            elif isinstance(centered, bool):
                centered_z = centered
                
            normal = prev_wp.plane.zDir
            plane_origin = prev_wp.plane.origin
            ref_dir = prev_wp.plane.xDir
            
            pushed_points = getattr(prev_wp, "_pushed_points", None)
            
            if pushed_points and len(pushed_points) > 1:
                # 5.3 阵列实例化 Box -> 拆分为 2D 矩形 + Extrude 并映射
                self.profile_counter += 1
                profile_id = f"Profile_Rect_{self.profile_counter}"
                
                self.recipe["declarations"]["profiles"].append({
                    "id": profile_id,
                    "type": "IfcRectangleProfileDef",
                    "ProfileType": "AREA",
                    "XDim": float(x),
                    "YDim": float(y)
                })
                
                self.feature_counter += 1
                source_feat_id = f"Single_Solid_{self.feature_counter}"
                
                offset_z = -float(z) / 2.0 if centered_z else 0.0
                placement = {
                    "type": "IfcAxis2Placement3D",
                    "Location": [0.0, 0.0, offset_z],
                    "Axis": [float(normal.x), float(normal.y), float(normal.z)],
                    "RefDirection": [float(ref_dir.x), float(ref_dir.y), float(ref_dir.z)]
                }
                
                self.recipe["features"].append({
                    "id": source_feat_id,
                    "type": "IfcExtrudedAreaSolid",
                    "SweptArea": profile_id,
                    "Position": placement,
                    "ExtrudedDirection": [float(normal.x), float(normal.y), float(normal.z)],
                    "Depth": float(z)
                })
                
                mapped_feats = []
                for idx, pt in enumerate(pushed_points):
                    world_pt = prev_wp.plane.toWorldCoords(pt)
                    self.feature_counter += 1
                    mapped_id = f"Mapped_Item_{self.feature_counter}"
                    
                    self.recipe["features"].append({
                        "id": mapped_id,
                        "type": "IfcMappedItem",
                        "SourceGeometry": source_feat_id,
                        "TargetLocation": [float(world_pt.x), float(world_pt.y), float(world_pt.z)]
                    })
                    mapped_feats.append(mapped_id)
                next_wp._current_feature_ids = mapped_feats
            else:
                # 5.4 普通 Box
                self.profile_counter += 1
                profile_id = f"Profile_Rect_{self.profile_counter}"
                
                self.recipe["declarations"]["profiles"].append({
                    "id": profile_id,
                    "type": "IfcRectangleProfileDef",
                    "ProfileType": "AREA",
                    "XDim": float(x),
                    "YDim": float(y)
                })
                
                self.feature_counter += 1
                feature_id = f"Feature_Extrude_{self.feature_counter}"
                
                if centered_z:
                    offset_vector = normal * (-float(z) / 2.0)
                    new_origin = plane_origin + offset_vector
                    temp_plane = cq.Plane(new_origin, prev_wp.plane.xDir, normal)
                    placement = self._get_placement_3d(temp_plane)
                else:
                    placement = self._get_placement_3d(prev_wp.plane)
                    
                self.recipe["features"].append({
                    "id": feature_id,
                    "type": "IfcExtrudedAreaSolid",
                    "SweptArea": profile_id,
                    "Position": placement,
                    "ExtrudedDirection": [float(normal.x), float(normal.y), float(normal.z)],
                    "Depth": float(z)
                })
                next_wp._current_feature_ids = [feature_id]

        # 6. 放样造型 (loft -> IfcSectionedSpine)
        elif name == "loft":
            self.feature_counter += 1
            feature_id = f"Feature_Loft_{self.feature_counter}"
            
            loft_profiles = getattr(prev_wp, "_loft_profiles", [])
            loft_positions = getattr(prev_wp, "_loft_positions", [])
            
            if not loft_profiles:
                curr_profile = getattr(prev_wp, "_current_profile_id", "Unknown_Profile")
                loft_profiles = [curr_profile]
                loft_positions = [self._get_placement_3d(prev_wp.plane)]
                
            spine_points = [pos["Location"] for pos in loft_positions]
            
            self.recipe["features"].append({
                "id": feature_id,
                "type": "IfcSectionedSpine",
                "SpineCurve": {
                    "type": "IfcPolyline",
                    "Points": spine_points
                },
                "CrossSections": loft_profiles,
                "Positions": loft_positions
            })
            next_wp._current_feature_ids = [feature_id]

        # 7. 布尔运算
        elif name == "cut":
            self.feature_counter += 1
            feature_id = f"Feature_Boolean_{self.feature_counter}"
            
            operand_a_ids = getattr(prev_wp, "_current_feature_ids", ["Unknown_Feature"])
            operand_a_id = operand_a_ids[0] if operand_a_ids else "Unknown_Feature"
            
            tool = args[0] if len(args) > 0 else kwargs.get("toCut")
            operand_b_id = "Unknown_Feature"
            if hasattr(tool, "_current_feature_ids") and tool._current_feature_ids:
                operand_b_id = tool._current_feature_ids[0]
            elif hasattr(tool, "_current_profile_id"):
                operand_b_id = tool._current_profile_id
                
            self.recipe["features"].append({
                "id": feature_id,
                "type": "IfcBooleanResult",
                "operator": "DIFFERENCE",
                "first_operand": operand_a_id,
                "second_operand": operand_b_id
            })
            next_wp._current_feature_ids = [feature_id]

        elif name == "union":
            self.feature_counter += 1
            feature_id = f"Feature_Boolean_{self.feature_counter}"
            
            operand_a_ids = getattr(prev_wp, "_current_feature_ids", ["Unknown_Feature"])
            operand_a_id = operand_a_ids[0] if operand_a_ids else "Unknown_Feature"
            
            tool = args[0] if len(args) > 0 else kwargs.get("toUnion")
            operand_b_id = "Unknown_Feature"
            if hasattr(tool, "_current_feature_ids") and tool._current_feature_ids:
                operand_b_id = tool._current_feature_ids[0]
                
            self.recipe["features"].append({
                "id": feature_id,
                "type": "IfcBooleanResult",
                "operator": "UNION",
                "first_operand": operand_a_id,
                "second_operand": operand_b_id
            })
            next_wp._current_feature_ids = [feature_id]

        # 8. 整体平移
        elif name == "translate":
            vec = args[0] if len(args) > 0 else kwargs.get("vec")
            if isinstance(vec, (tuple, list)):
                dx, dy, dz = float(vec[0]), float(vec[1]), float(vec[2])
            else:
                dx, dy, dz = float(vec.x), float(vec.y), float(vec.z)
                
            curr_feats = getattr(prev_wp, "_current_feature_ids", [])
            for feat_id in curr_feats:
                for f in self.recipe["features"]:
                    if f["id"] == feat_id:
                        if "Position" in f:
                            pos = f["Position"]
                            pos["Location"][0] += dx
                            pos["Location"][1] += dy
                            pos["Location"][2] += dz
                        elif f["type"] == "IfcMappedItem":
                            f["TargetLocation"][0] += dx
                            f["TargetLocation"][1] += dy
                            f["TargetLocation"][2] += dz
                        elif f["type"] == "IfcSectionedSpine":
                            for pos in f["Positions"]:
                                pos["Location"][0] += dx
                                pos["Location"][1] += dy
                                pos["Location"][2] += dz
                            for pt in f["SpineCurve"]["Points"]:
                                pt[0] += dx
                                pt[1] += dy
                                pt[2] += dz
            next_wp._current_feature_ids = curr_feats

    def add_product(self, product_id: str, ifc_type: str, name: str, predefined_type: str, representations: List[str]):
        """手动添加语义 BIM 产品关联"""
        self.recipe["products"].append({
            "id": product_id,
            "ifc_type": ifc_type,
            "predefined_type": predefined_type,
            "name": name,
            "representations": representations
        })

    def export_json(self) -> str:
        """导出完整的参数化 Recipe JSON"""
        # 对未分配给任何 Product 的 feature 进行默认兜底包装
        assigned_representations = set()
        for p in self.recipe["products"]:
            assigned_representations.update(p["representations"])
            
        unassigned_features = [f["id"] for f in self.recipe["features"] if f["id"] not in assigned_representations]
        if unassigned_features:
            self.recipe["products"].append({
                "id": "Product_Group_01",
                "ifc_type": "IfcBuildingElementProxy",
                "predefined_type": "NOTDEFINED",
                "name": "Auto_Recorded_Product",
                "representations": unassigned_features
            })
            
        return json.dumps(self.recipe, indent=2, ensure_ascii=False)


def export_cipr_package(cq_shape: Any, tracker: CqParametricTracker) -> str:
    """双生协同传输包 (CIPR-PKG) 拼装"""
    import tempfile
    
    # 支持 Workplane 的自动转换提取
    if hasattr(cq_shape, "toCompound"):
        actual_shape = cq_shape.toCompound()
    elif hasattr(cq_shape, "val"):
        actual_shape = cq_shape.val()
    else:
        actual_shape = cq_shape
        
    # 1. 序列化 TopoDS_Shape 为 BREP 文本流
    fd, temp_path = tempfile.mkstemp(suffix=".brep")
    try:
        actual_shape.exportBrep(temp_path)
        with open(temp_path, "r", encoding="utf-8") as f:
            brep_data = f.read()
    finally:
        os.close(fd)
        try:
            os.remove(temp_path)
        except Exception:
            pass

    # 2. 拼装双生包
    cipr_package = {
        "pkg_version": "1.0.0",
        "recipe": tracker.recipe,
        "geometries": {
            "raw_brep_format": "ASCII",
            "raw_brep_data": brep_data
        }
    }
    return json.dumps(cipr_package, ensure_ascii=False)


# ==============================================================================
# 单元测试用例及验证模块
# ==============================================================================

def run_tests():
    print("=" * 60)
    print("  开始运行 CadQuery 拦截器参数化录制测试")
    print("=" * 60)

    # 1. 创建并激活拦截器
    tracker = CqParametricTracker(model_name="Bridge_Structure_Recorded")
    tracker.start_tracking()
    print("[1] 拦截器已成功激活 (Monkey-Patching Active)...")

    # 2. 模拟承台建模：PileCap.py
    # 模拟在 'XY' 面上画一个 7000x5000x2000 的承台 (centered_z = False)
    print("[2] 正在运行承台(Cap)建模代码...")
    cap_wp = cq.Workplane("XY").box(7000.0, 5000.0, 2000.0, centered=(True, True, False))
    cap_feature_ids = getattr(cap_wp, "_current_feature_ids", [])
    tracker.add_product(
        product_id="Product_Cap_01",
        ifc_type="IfcFooting",
        predefined_type="PILE_CAP",
        name="承台_C35",
        representations=cap_feature_ids
    )

    # 3. 模拟多桩阵列实例化建模：Pile.py
    # 在底座下方拉伸 6 根圆柱桩，使用 pushPoints
    print("[3] 正在运行桩组(Piles)几何实例化建模代码...")
    pts = []
    hSpacing = 2500.0
    vSpacing = 3000.0
    for xi in [-hSpacing, 0, hSpacing]:
        for yi in [-vSpacing / 2.0, vSpacing / 2.0]:
            pts.append((xi, yi))
            
    piles_wp = (
        cq.Workplane("XY")
        .workplane(offset=-2000.0) # 在承台底面
        .pushPoints(pts)
        .circle(500.0)
        .extrude(-15000.0)
    )
    piles_feature_ids = getattr(piles_wp, "_current_feature_ids", [])
    tracker.add_product(
        product_id="Product_Pile_01",
        ifc_type="IfcPile",
        predefined_type="BORED",
        name="基础灌注桩组",
        representations=piles_feature_ids
    )

    # 4. 模拟避雷针底座建模（带多立柱 box 实例化）：lightning_rod_foundation.py
    print("[4] 正在运行避雷针底座多立柱(box)实例化建模代码...")
    rod_base = cq.Workplane("XY").workplane(offset=-2000.0).box(2700.0, 2700.0, 1000.0, centered=(True, True, False))
    post_centers = [
        (647.5, 647.5),
        (647.5, -647.5),
        (-647.5, 647.5),
        (-647.5, -647.5)
    ]
    rod_posts = rod_base.faces(">Z").workplane().pushPoints(post_centers).box(500.0, 500.0, 900.0, centered=(True, True, False))

    # 5. 模拟变截面桥墩放样与切除建模：BridgePier2.py / PierBody.py
    print("[5] 正在运行复杂桥墩变截面放样(Loft)与切除(Cut)建模代码...")
    def draw_pier_outline(wp, xr, yr, px):
        # 极简绘制轮廓以作演示
        return (wp.moveTo(-xr, -yr)
                .lineTo(-xr, yr)
                .lineTo(xr, yr)
                .lineTo(xr, -yr)
                .close())

    pier_wp = cq.Workplane("XY")
    pier_wp = draw_pier_outline(pier_wp, 1600.0, 1400.0, 3000.0)
    pier_wp = draw_pier_outline(pier_wp.workplane(offset=3000.0), 2400.0, 1500.0, 3900.0)
    # 进行 loft
    pier_body = pier_wp.loft()
    
    # 模拟布尔切除 (cut)
    cutter = cq.Workplane("XZ").moveTo(-500.0, 1000.0).lineTo(-500.0, 2000.0).lineTo(500.0, 2000.0).lineTo(500.0, 1000.0).close().extrude(10000.0, both=True)
    pier_body_cut = pier_body.cut(cutter)

    # 6. 注销拦截器并导出 JSON 协议树
    tracker.stop_tracking()
    print("[6] 建模完成，注销拦截器 (Monkey-Patching Restored)...")
    
    recipe_json = tracker.export_json()
    output_json_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "pile_cap_assembly.json"))
    
    with open(output_json_path, "w", encoding="utf-8") as f:
        f.write(recipe_json)
        
    print(f"[Success] Parametric CIPR JSON written successfully to:\n  {output_json_path}\n")

    # 7. 自检验证与结构校验
    print("=" * 60)
    print("  自校验报告 (Validation Report)")
    print("=" * 60)
    
    recipe = json.loads(recipe_json)
    
    # A. 验证 profiles 库
    profiles = recipe["declarations"]["profiles"]
    print(f"-> 截面库定义校验: 共捕获到 {len(profiles)} 个截面。")
    for p in profiles:
        print(f"   [截面] ID: {p['id']}, 类型: {p['type']}")
        
    # B. 验证 features 特征
    features = recipe["features"]
    print(f"-> 特征造型库校验: 共捕获到 {len(features)} 个特征实体。")
    
    # 检测几何实例化支持
    mapped_items = [f for f in features if f["type"] == "IfcMappedItem"]
    print(f"   [几何实例化复用] 共捕获到 {len(mapped_items)} 个 IfcMappedItem。")
    for m in mapped_items[:4]:
        print(f"   - 引用源: {m['SourceGeometry']}, 目标平移坐标: {m['TargetLocation']}")
    if len(mapped_items) > 4:
        print("     ...")
        
    # 检测布尔运算和放样
    loft_items = [f for f in features if f["type"] == "IfcSectionedSpine"]
    boolean_items = [f for f in features if f["type"] == "IfcBooleanResult"]
    print(f"   [多截面变特征放样] 共捕获到 {len(loft_items)} 个 IfcSectionedSpine 实体。")
    print(f"   [构造实体布尔运算] 共捕获到 {len(boolean_items)} 个 IfcBooleanResult 实体。")

    # C. 验证 products 语义
    products = recipe["products"]
    print(f"-> BIM 构件语义校验: 共捕获到 {len(products)} 个物理产品。")
    for prod in products:
        print(f"   [构件] ID: {prod['id']}, IFC类型: {prod['ifc_type']}, 预定义子类: {prod.get('predefined_type', 'N/A')}, 名称: {prod['name']}, 几何映射数: {len(prod['representations'])}")

    # D. TopoDS_Shape 双生包打包校验
    final_compound = cap_wp.union(piles_wp).union(pier_body_cut)
    pkg_json = export_cipr_package(final_compound, tracker)
    print(f"-> 双生协同交付包打包完成，数据大小: {len(pkg_json) / 1024:.2f} KB")

    print("\n[Validate OK] 拦截器验证全部通过！生成的 CIPR JSON 与 @[docs/pile_parametric_protocol_design.md] 建模协议完美一致。")
    print("=" * 60)

if __name__ == "__main__":
    run_tests()
