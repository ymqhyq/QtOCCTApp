"""STEP 原厂精确实体导入与 RCIF 封装管线 (step2rcif.py)
对应方案 WBS 2.2 与必测案例 2 (梁.step -> BoxGirder_1.0.0.rcif)

严格契约 (RCIF v1.4.2 §3.2)：
- identity.sourceMode: "native-brep"
- geometry.exactBrep: true
- 严禁包含 recipe 字段
- 严禁包含非规范 quality.tier 字段
- 规范化 PRODUCT 名称与 GLB 节点名称为 GirderBody
"""

from __future__ import annotations

import os
import re
import sys
import uuid
import shutil
import datetime
from typing import Tuple
import numpy as np

from OCP.STEPControl import STEPControl_Reader
from OCP.IFSelect import IFSelect_RetDone
from OCP.TopoDS import TopoDS, TopoDS_Compound
from OCP.BRep import BRep_Builder
from OCP.gp import gp_Pnt
from OCP.BRepBuilderAPI import (
    BRepBuilderAPI_MakeFace,
    BRepBuilderAPI_MakePolygon,
    BRepBuilderAPI_MakeSolid,
    BRepBuilderAPI_Sewing,
)
from OCP.BRepCheck import BRepCheck_Analyzer, BRepCheck_Status
from OCP.TopExp import TopExp_Explorer, TopExp
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopAbs import TopAbs_FACE, TopAbs_VERTEX, TopAbs_SHELL, TopAbs_REVERSED
from OCP.BRep import BRep_Tool
from OCP.BRepAdaptor import BRepAdaptor_Surface
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.GProp import GProp_GProps
from OCP.BRepGProp import BRepGProp
from OCP.TopLoc import TopLoc_Location
from OCP.Bnd import Bnd_Box
from OCP.BRepBndLib import BRepBndLib

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import rcifc
from glb_writer import write_glb_parts


def _read_faceted_brep(step_path: str):
    """Fallback for FACETED_BREP files whose faces the OCP STEP reader drops.

    Some supplier STEP files are polygonal FACETED_BREP/CLOSED_SHELL data.
    The OpenCascade reader can return an empty shell for these.  Rebuild the
    triangles as planar faces, sew them, and keep every resulting solid in a
    compound so volume/bbox/mesh are still meaningful.
    """
    with open(step_path, encoding='utf-8', errors='replace') as handle:
        text = handle.read()
    if 'FACETED_BREP' not in text.upper():
        return None

    points = {}
    for match in re.finditer(
        r"#(\d+)\s*=\s*CARTESIAN_POINT\s*\(\s*'[^']*'\s*,\s*\(\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*\)\s*\)",
        text,
    ):
        points[int(match.group(1))] = (float(match.group(2)), float(match.group(3)), float(match.group(4)))
    loops = {}
    for match in re.finditer(r"#(\d+)\s*=\s*POLY_LOOP\s*\(\s*'[^']*'\s*,\s*\(([^)]*)\)\s*\)", text):
        loops[int(match.group(1))] = [int(x) for x in re.findall(r'#(\d+)', match.group(2))]
    bounds = {}
    for match in re.finditer(
        r"#(\d+)\s*=\s*FACE_OUTER_BOUND\s*\(\s*'[^']*'\s*,\s*#(\d+)\s*,\s*\.(T|F)\.\s*\)",
        text,
    ):
        bounds[int(match.group(1))] = (int(match.group(2)), match.group(3) == 'T')
    faces = {}
    for match in re.finditer(r"#(\d+)\s*=\s*FACE\s*\(\s*'[^']*'\s*,\s*\(([^)]*)\)\s*\)", text):
        faces[int(match.group(1))] = [int(x) for x in re.findall(r'#(\d+)', match.group(2))]
    if not points or not loops or not faces:
        return None

    triangles = []
    for face_bounds in faces.values():
        for bound_id in face_bounds:
            bound = bounds.get(bound_id)
            loop = loops.get(bound[0]) if bound else None
            if not loop:
                continue
            pts = [points[i] for i in loop if i in points]
            if len(pts) < 3:
                continue
            # 去掉闭合点重复
            deduped = []
            for pt in pts:
                if not deduped or pt != deduped[-1]:
                    deduped.append(pt)
            pts = deduped
            if len(pts) < 3:
                continue
            for index in range(1, len(pts) - 1):
                tri = [pts[0], pts[index], pts[index + 1]]
                if bound and not bound[1]:
                    tri.reverse()
                triangles.append(tri)
    if not triangles:
        return None

    sewing = BRepBuilderAPI_Sewing(1e-4)
    for tri in triangles:
        poly = BRepBuilderAPI_MakePolygon()
        for point in tri:
            poly.Add(gp_Pnt(*point))
        poly.Close()
        if not poly.IsDone():
            continue
        maker = BRepBuilderAPI_MakeFace(poly.Wire(), True)
        if maker.IsDone():
            sewing.Add(maker.Face())
    sewing.Perform()
    sewed = sewing.SewedShape()
    shells = []
    explorer = TopExp_Explorer(sewed, TopAbs_SHELL)
    while explorer.More():
        shells.append(TopoDS.Shell_s(explorer.Current()))
        explorer.Next()
    if not shells:
        return None

    builder = BRep_Builder()
    compound = TopoDS_Compound()
    builder.MakeCompound(compound)
    for shell in shells:
        maker = BRepBuilderAPI_MakeSolid(shell)
        builder.Add(compound, maker.Solid() if maker.IsDone() else shell)
    return compound


def _face_count(shape) -> int:
    explorer = TopExp_Explorer(shape, TopAbs_FACE)
    count = 0
    while explorer.More():
        count += 1
        explorer.Next()
    return count


def inspect_and_mesh_step(step_path: str) -> dict:
    reader = STEPControl_Reader()
    status = reader.ReadFile(step_path)
    if status != IFSelect_RetDone:
        raise ValueError(f"无法读取 STEP 文件: {step_path} (status={status})")

    reader.TransferRoots()
    shape = reader.OneShape()

    # 供应商的 FACETED_BREP 可能被 STEP reader 读成空 shell；
    # 此时从几何实体重建三角面/壳/实体。
    if _face_count(shape) == 0:
        rebuilt = _read_faceted_brep(step_path)
        if rebuilt is not None:
            shape = rebuilt
            print(f"[INFO] FACETED_BREP fallback: rebuilt {_face_count(shape)} faces")

    # 提取 Shell 并构建 Solid (ISSUE-015: 严格校验缝合状态)
    if shape.ShapeType() == TopAbs_SHELL:
        shell = TopoDS.Shell_s(shape)
        maker = BRepBuilderAPI_MakeSolid(shell)
        if not maker.IsDone():
            raise RuntimeError("Shell 壳体缝合失败，无法构建有效 Solid 实体")
        solid = maker.Solid()
    else:
        solid = shape
        shell = None

    # 顶点统计与精确极值包围盒计算（使用唯一映射去重）
    vmap = TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(solid, TopAbs_VERTEX, vmap)
    unique_v_count = vmap.Extent()

    vertices = []
    for i in range(1, unique_v_count + 1):
        v = TopoDS.Vertex_s(vmap.FindKey(i))
        pt = BRep_Tool.Pnt_s(v)
        vertices.append([pt.X(), pt.Y(), pt.Z()])
    V_arr = np.array(vertices, dtype=float)

    # ISSUE-015: 结合 BRepBndLib.AddOptimal_s 与顶点极值取并集，防止曲面凸出被低估
    bnd = Bnd_Box()
    BRepBndLib.AddOptimal_s(solid, bnd, False, False)
    bx0, by0, bz0, bx1, by1, bz1 = bnd.Get()

    xmin = min(float(bx0), float(V_arr[:, 0].min()))
    ymin = min(float(by0), float(V_arr[:, 1].min()))
    zmin = min(float(bz0), float(V_arr[:, 2].min()))
    xmax = max(float(bx1), float(V_arr[:, 0].max()))
    ymax = max(float(by1), float(V_arr[:, 1].max()))
    zmax = max(float(bz1), float(V_arr[:, 2].max()))

    # 统计面数量与曲面类型
    ex_f = TopExp_Explorer(solid, TopAbs_FACE)
    face_count = 0
    surface_types = {}
    while ex_f.More():
        f = TopoDS.Face_s(ex_f.Current())
        surf = BRepAdaptor_Surface(f)
        stype = surf.GetType().name
        surface_types[stype] = surface_types.get(stype, 0) + 1
        face_count += 1
        ex_f.Next()

    # 拓扑封闭与流形检查
    ana = BRepCheck_Analyzer(solid)
    shell_valid = True
    if shell is not None:
        s_res = ana.Result(shell)
        if s_res:
            statuses = [s for s in s_res.Status()]
            shell_valid = all(s == BRepCheck_Status.BRepCheck_NoError for s in statuses)

    solid_res = ana.Result(solid)
    solid_valid = True
    if solid_res:
        statuses = [s for s in solid_res.Status()]
        solid_valid = all(s == BRepCheck_Status.BRepCheck_NoError for s in statuses)

    # 实体质量与体积积分计算
    gprops = GProp_GProps()
    BRepGProp.VolumeProperties_s(solid, gprops)
    volume_mm3 = gprops.Mass()
    volume_m3 = round(volume_mm3 / 1e9, 3)

    # 细化三角剖分 (偏转角 0.1 rad, 线性偏转 5.0 mm)
    mesher = BRepMesh_IncrementalMesh(solid, 5.0, False, 0.1, True)
    mesher.Perform()

    mesh_verts = []
    mesh_tris = []
    ex_f_mesh = TopExp_Explorer(solid, TopAbs_FACE)
    vert_offset = 0

    while ex_f_mesh.More():
        f = TopoDS.Face_s(ex_f_mesh.Current())
        loc = TopLoc_Location()
        triangulation = BRep_Tool.Triangulation_s(f, loc)
        if triangulation is not None:
            trsf = loc.Transformation()
            nb_nodes = triangulation.NbNodes()
            nb_tris = triangulation.NbTriangles()
            is_reversed = (f.Orientation() == TopAbs_REVERSED)

            for i in range(1, nb_nodes + 1):
                p = triangulation.Node(i).Transformed(trsf)
                mesh_verts.append([p.X(), p.Y(), p.Z()])

            for i in range(1, nb_tris + 1):
                tri = triangulation.Triangle(i)
                n1, n2, n3 = tri.Get()
                if is_reversed:
                    mesh_tris.append([vert_offset + n1 - 1, vert_offset + n3 - 1, vert_offset + n2 - 1])
                else:
                    mesh_tris.append([vert_offset + n1 - 1, vert_offset + n2 - 1, vert_offset + n3 - 1])

            vert_offset += nb_nodes
        ex_f_mesh.Next()

    def _norm0(v: float) -> float:
        r = round(float(v), 2)
        return 0.0 if abs(r) == 0.0 else r

    return {
        "vertex_count": unique_v_count,
        "face_count": face_count,
        "surface_types": surface_types,
        "shell_valid": shell_valid,
        "solid_valid": solid_valid,
        "volume_m3": volume_m3,
        "bbox": {
            "x": [_norm0(xmin), _norm0(xmax)],
            "y": [_norm0(ymin), _norm0(ymax)],
            "z": [_norm0(zmin), _norm0(zmax)],
        },
        "mesh_verts": mesh_verts,
        "mesh_tris": mesh_tris,
    }


def normalize_step_product_name(input_step: str, output_step: str, new_name: str = "GirderBody") -> None:
    """消除原厂无意义名称 (如 Open CASCADE STEP translator 8.0 1)，重命名为业务语义名称。"""
    with open(input_step, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    # 1. 替换 PRODUCT 实体名称
    pattern_prod = r"PRODUCT\s*\(\s*'[^']*'\s*,\s*'[^']*'\s*,"
    replacement_prod = f"PRODUCT('{new_name}','{new_name}',"
    new_content, n1 = re.subn(pattern_prod, replacement_prod, content)

    # 2. 替换 PRODUCT_DEFINITION_FORMATION 名称
    pattern_pdf = r"PRODUCT_DEFINITION_FORMATION\s*\(\s*'[^']*'\s*,\s*'[^']*'\s*,"
    replacement_pdf = f"PRODUCT_DEFINITION_FORMATION('{new_name}_REV1','{new_name}',"
    new_content, n2 = re.subn(pattern_pdf, replacement_pdf, new_content)

    with open(output_step, "w", encoding="utf-8") as f:
        f.write(new_content)


def convert_step_to_rcif(
    input_step_path: str,
    output_dir: str,
    family_id: str = "BoxGirder",
    revision: str = "1.0.0",
    node_name: str = "GirderBody",
    material: str = "C50",
    semantic_code: str = "Bridge.Superstructure.BoxGirder",
    component_name: str | None = None,
    drawing_code: str = "通桥（2016）2322A-Ⅱ-1",
) -> Tuple[str, dict]:
    """将 STEP 文件端到端转换为 RCIF v1.4.2 构件包。"""
    if not component_name:
        component_name = "预制双线简支箱梁 31.5m" if family_id == "BoxGirder" else family_id
    # 规范化目标文件与输出目录，防止将文件路径误建为目录
    if output_dir.lower().endswith(".rcif"):
        output_rcif = os.path.abspath(output_dir)
        target_dir = os.path.dirname(output_rcif)
    else:
        target_dir = os.path.abspath(output_dir)
        output_rcif = os.path.join(target_dir, f"{family_id}_{revision}.rcif")

    stage_dir = os.path.join(target_dir, f".stage_step_{uuid.uuid4().hex[:8]}")
    if os.path.exists(stage_dir):
        shutil.rmtree(stage_dir, ignore_errors=True)
    os.makedirs(stage_dir, exist_ok=True)

    try:
        geom_dir = os.path.join(stage_dir, "geometry")
        disp_dir = os.path.join(stage_dir, "display")
        os.makedirs(geom_dir, exist_ok=True)
        os.makedirs(disp_dir, exist_ok=True)

        # 1. 解析几何与三角网格
        geo_info = inspect_and_mesh_step(input_step_path)

        # 2. 导出规范化 STEP 文件
        dest_step = os.path.join(geom_dir, "default.step")
        normalize_step_product_name(input_step_path, dest_step, new_name=node_name)

        # 3. 导出预览 GLB 文件
        dest_glb = os.path.join(disp_dir, "preview.glb")
        part_item = {
            "name": node_name,
            "verts": geo_info["mesh_verts"],
            "tris": geo_info["mesh_tris"],
            "color": (0.68, 0.72, 0.75),
            "material": material,
            "extras": {
                "semanticCode": semantic_code,
                "volumeM3": geo_info["volume_m3"],
            },
        }
        write_glb_parts([part_item], dest_glb)

        # 4. 4条测试向量自检
        spec_vectors = rcifc.verify_vectors()

        # 5. 构建 component.json (严格遵循 RCIF v1.4.2 native-brep 契约，统一字段契约)
        span_len = float(geo_info["bbox"]["x"][1] - geo_info["bbox"]["x"][0])
        deck_w = float(geo_info["bbox"]["y"][1] - geo_info["bbox"]["y"][0])
        g_height = float(geo_info["bbox"]["z"][1] - geo_info["bbox"]["z"][0])

        # 计算自适应支座端口偏移量 (ISSUE-020)
        dx_seat = round(min(1500.0, max(500.0, span_len * 0.0476)), 1)
        dy_seat = round(min(2200.0, max(800.0, deck_w * 0.1746)), 1)

        component_json = {
            "format": "cbf-component",
            "schemaVersion": "1.4.2",
            "identity": {
                "componentId": str(uuid.uuid4()),
                "familyId": family_id,
                "revision": revision,
                "name": component_name,
                "category": "BridgeSuperstructure",
                "semanticCode": semantic_code,
                "semanticProfileHint": "Railway-CRBIM-IFC4X3@1.4",
                "sourceMode": "native-brep",
                "placement": {
                    "modes": ["point"],
                    "orientation": "azimuth",
                },
                "drawingCode": drawing_code,
                "designSpeed": "350km/h",
                "applicableSpan": [f"{span_len/1000:.1f}m简支箱梁"],
            },
            "environment": {
                "units": {"length": "mm", "angle": "rad"},
                "tolerance": {"linear": 0.01, "angular": 0.001},
            },
            "parameters": {
                "schema": [
                    {
                        "name": "SpanLength",
                        "label": "计算跨度/梁长",
                        "type": "real",
                        "default": span_len,
                        "unit": "mm",
                        "access": "out",
                    },
                    {
                        "name": "DeckWidth",
                        "label": "桥面总宽",
                        "type": "real",
                        "default": deck_w,
                        "unit": "mm",
                        "access": "out",
                    },
                    {
                        "name": "GirderHeight",
                        "label": "梁体结构高度",
                        "type": "real",
                        "default": g_height,
                        "unit": "mm",
                        "access": "out",
                    },
                    {
                        "name": "NetVolume",
                        "label": "梁体净方量",
                        "type": "real",
                        "default": geo_info["volume_m3"],
                        "unit": "m3",
                        "access": "out",
                    },
                ],
                "presets": [
                    {
                        "modelNumber": f"{family_id}-{span_len/1000:.1f}m-Standard",
                        "label": f"{span_len/1000:.1f}m 双线预制简支箱梁",
                        "drawingCode": drawing_code,
                        "params": {
                            "SpanLength": span_len,
                            "DeckWidth": deck_w,
                            "GirderHeight": g_height,
                            "NetVolume": geo_info["volume_m3"],
                        },
                        "isPrebaked": True,
                    }
                ],
            },
            "geometry": {
                "exactBrep": True,
                "defaultFormat": "step",
                "defaultFile": "geometry/default.step",
                "bounds": geo_info["bbox"],
                "volumeM3": geo_info["volume_m3"],
            },
            "shapes": {
                node_name: {
                    "label": "简支箱梁梁体",
                    "code": semantic_code,
                    "geomType": "solid",
                    "material": material,
                    "node": node_name,
                }
            },
            "ports": {
                "DeckCenterStart": {
                    "label": "梁顶起点中心点",
                    "origin": [geo_info["bbox"]["x"][0], 0.0, geo_info["bbox"]["z"][1]],
                    "axis": [0.0, 0.0, 1.0],
                    "refDirection": [1.0, 0.0, 0.0],
                },
                "DeckCenterEnd": {
                    "label": "梁顶终点中心点",
                    "origin": [geo_info["bbox"]["x"][1], 0.0, geo_info["bbox"]["z"][1]],
                    "axis": [0.0, 0.0, 1.0],
                    "refDirection": [1.0, 0.0, 0.0],
                },
                "BearingSeatLeftStart": {
                    "label": "起点左侧支座中心",
                    "origin": [geo_info["bbox"]["x"][0] + dx_seat, -dy_seat, geo_info["bbox"]["z"][0]],
                    "axis": [0.0, 0.0, 1.0],
                    "refDirection": [1.0, 0.0, 0.0],
                },
                "BearingSeatRightStart": {
                    "label": "起点右侧支座中心",
                    "origin": [geo_info["bbox"]["x"][0] + dx_seat, dy_seat, geo_info["bbox"]["z"][0]],
                    "axis": [0.0, 0.0, 1.0],
                    "refDirection": [1.0, 0.0, 0.0],
                },
                "BearingSeatLeftEnd": {
                    "label": "终点左侧支座中心",
                    "origin": [geo_info["bbox"]["x"][1] - dx_seat, -dy_seat, geo_info["bbox"]["z"][0]],
                    "axis": [0.0, 0.0, 1.0],
                    "refDirection": [1.0, 0.0, 0.0],
                },
                "BearingSeatRightEnd": {
                    "label": "终点右侧支座中心",
                    "origin": [geo_info["bbox"]["x"][1] - dx_seat, dy_seat, geo_info["bbox"]["z"][0]],
                    "axis": [0.0, 0.0, 1.0],
                    "refDirection": [1.0, 0.0, 0.0],
                },
            },


            "qa": {
                "specVectors": spec_vectors,
                "vertexCount": geo_info["vertex_count"],
                "faceCount": geo_info["face_count"],
                "surfaceTypes": geo_info["surface_types"],
                "shellValid": geo_info["shell_valid"],
                "solidValid": geo_info["solid_valid"],
                "meshTriangles": len(geo_info["mesh_tris"]),
            },
            "changelog": [
                {
                    "version": revision,
                    "date": datetime.date.today().isoformat(),
                    "author": "RCIF-DSH-Studio",
                    "type": "initial",
                    "changes": [
                        f"通过 RCIF 智能创族平台由 {os.path.basename(input_step_path)} 生成",
                        f"几何特征: 梁长={span_len}mm, 梁宽={deck_w}mm, 梁高={g_height}mm, 方量={geo_info['volume_m3']}m3",
                        f"曲面组成: {geo_info['face_count']}个高级面 ({geo_info['surface_types']})",
                        f"PRODUCT 节点规范化为 {node_name}，材质绑定 {material}",
                    ],
                }
            ],
        }

        # 6. 打包产出 .rcif
        pkg_path, pkg_sha256 = rcifc.build_rcif_package(stage_dir, output_rcif, component_json)

        result_info = {
            "output_rcif": pkg_path,
            "package_sha256": pkg_sha256,
            "component_json": component_json,
            "geo_info": {
                "bbox": geo_info["bbox"],
                "volume_m3": geo_info["volume_m3"],
                "vertex_count": geo_info["vertex_count"],
                "face_count": geo_info["face_count"],
            },
        }
        return pkg_path, result_info
    finally:
        # 清理临时 staging 目录 (ISSUE-009)
        shutil.rmtree(stage_dir, ignore_errors=True)


def main():
    """提供标准 CLI 命令行入口 (ISSUE-021)。"""
    import argparse
    parser = argparse.ArgumentParser(description="STEP 原厂精确实体导入与 RCIF 封装管线 (step2rcif.py)")
    parser.add_argument("step_path", help="输入 STEP 文件路径")
    parser.add_argument("output_dir", help="输出构件包目录")
    parser.add_argument("--family-id", default="BoxGirder", help="构件族标识")
    parser.add_argument("--revision", default="1.0.0", help="版本号")
    parser.add_argument("--name", default=None, help="构件显示名称（默认按族 ID）")
    parser.add_argument("--node-name", default="GirderBody", help="主实体节点名")
    parser.add_argument("--material", default="C50", help="材质等级")
    args = parser.parse_args()

    try:
        pkg, info = convert_step_to_rcif(
            input_step_path=args.step_path,
            output_dir=args.output_dir,
            family_id=args.family_id,
            revision=args.revision,
            node_name=args.node_name,
            material=args.material,
            component_name=args.name,
        )
        print(f"[OK] 生成成功: {pkg} (SHA256={info['package_sha256'][:16]}...)")
        sys.exit(0)
    except Exception as exc:
        print(f"[ERROR] 转换失败: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

