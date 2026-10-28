"""IFC -> RCIF 原生网格构件转换器 (ifc2rcif.py)

定位：
- IFC 是“已建成的语义模型”，无法自动还原参数化驱动；本工具如实产出
  `sourceMode = "mesh-only"` 的原生构件（命名部位 + 预览网格 + 包围盒 + 体积），
  不冒充精确 B-Rep / 参数化能力。
- 输出符合 RCIF v1.4.2 包结构：component.json + display/preview.glb + checksums.json。

用法：
  python ifc2rcif.py model.ifc out/Family_1.0.0.rcif --family-id Family --revision 1.0.0
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
import uuid

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import glb_writer  # noqa: E402
import rcifc  # noqa: E402

MAX_PARTS = 250
PALETTE = [
    (0.62, 0.66, 0.70),
    (0.42, 0.55, 0.68),
    (0.72, 0.62, 0.50),
    (0.50, 0.68, 0.58),
    (0.68, 0.52, 0.58),
    (0.55, 0.58, 0.72),
]


def _safe_name(raw: str, fallback: str) -> str:
    text = str(raw or "").strip()
    if not text:
        return fallback
    cleaned = "".join(ch if ch.isalnum() or ch in "._-/ " else "_" for ch in text)
    cleaned = cleaned.replace("/", "_")[:64].strip()
    return cleaned or fallback


def _mesh_volume_m3(verts: np.ndarray, faces: np.ndarray) -> float:
    """三角网格体积（mm^3 -> m^3）。

    闭合且法向一致的网格用有向散度求和；IFC 常见法向不一致导致抵消，
    此时退化为逐三角形绝对值/2 的保守估计，避免输出 0。
    """
    if len(verts) == 0 or len(faces) == 0:
        return 0.0
    tri = verts[faces]
    v0, v1, v2 = tri[:, 0], tri[:, 1], tri[:, 2]
    signed = np.einsum("ij,ij->i", v0, np.cross(v1, v2)) / 6.0
    signed_total = abs(float(signed.sum()))
    absolute_total = float(np.abs(signed).sum())
    volume_mm3 = signed_total if signed_total > absolute_total * 0.05 else absolute_total / 2.0
    return round(volume_mm3 / 1e9, 3)


def _detect_mm_scale(declared_scale: float, raw_extent: float) -> float:
    """IFC 单位声明与实际坐标不一致时的单位纠偏。

    - 声明为 mm（scale≈1）但模型包络 < 1000 → 实际坐标多半是米，×1000。
    - 声明为 m（scale≈1000）→ 直接 ×1000。
    """
    if declared_scale >= 100.0:
        return declared_scale
    if declared_scale <= 1.0 and raw_extent < 1000.0:
        return 1000.0
    return 1.0


def convert_ifc_to_rcif(
    ifc_path: str,
    output_rcif: str,
    family_id: str = "ImportedIFC",
    revision: str = "1.0.0",
    name: str | None = None,
) -> tuple[str, dict]:
    import ifcopenshell
    import ifcopenshell.geom

    ifc_path = os.path.abspath(ifc_path)
    output_rcif = os.path.abspath(output_rcif)
    if not os.path.isfile(ifc_path):
        raise FileNotFoundError(f"IFC 文件不存在: {ifc_path}")
    if not output_rcif.lower().endswith(".rcif"):
        output_rcif = os.path.join(output_rcif, f"{family_id}_{revision}.rcif")

    ifc = ifcopenshell.open(ifc_path)
    settings = ifcopenshell.geom.settings()
    try:
        settings.set(settings.USE_WORLD_COORDS, True)
    except Exception:
        pass

    # IFC 坐标使用项目单位（常见为 m 或 mm）；RCIF 统一使用 mm。
    try:
        import ifcopenshell.util.unit

        declared_scale = float(ifcopenshell.util.unit.calculate_unit_scale(ifc)) * 1000.0
    except Exception:
        declared_scale = 1000.0

    products = [p for p in ifc.by_type("IfcProduct") if getattr(p, "Representation", None)]
    raw_parts: list[dict] = []
    used_names: set[str] = set()
    raw_min = np.array([np.inf, np.inf, np.inf])
    raw_max = np.array([-np.inf, -np.inf, -np.inf])

    for product in products:
        if len(raw_parts) >= MAX_PARTS:
            break
        try:
            shape = ifcopenshell.geom.create_shape(settings, product)
        except Exception:
            continue
        geometry = shape.geometry
        verts = np.asarray(geometry.verts, dtype=np.float64)
        faces = np.asarray(geometry.faces, dtype=np.int64)
        if verts.size == 0 or faces.size == 0:
            continue
        verts = verts.reshape(-1, 3)
        faces = faces.reshape(-1, 3)

        part_name = _safe_name(getattr(product, "Name", None), f"{(product.is_a() or 'Part')}_{product.id()}")
        base_name = part_name
        suffix = 1
        while part_name in used_names:
            suffix += 1
            part_name = f"{base_name}_{suffix}"
        used_names.add(part_name)

        material = "IFC"
        try:
            material = product.HasAssociations[0].RelatingMaterial.Name or "IFC"
        except Exception:
            pass

        raw_parts.append({
            "name": part_name,
            "verts": verts,
            "faces": faces,
            "material": str(material)[:60],
        })
        raw_min = np.minimum(raw_min, verts.min(axis=0))
        raw_max = np.maximum(raw_max, verts.max(axis=0))

    if not raw_parts:
        raise RuntimeError("IFC 中没有可三角化的产品几何（IfcProduct.Representation 为空）")

    raw_extent = float(np.max(raw_max - raw_min))
    mm_scale = _detect_mm_scale(declared_scale, raw_extent)

    parts: list[dict] = []
    total_volume = 0.0
    all_min = np.array([np.inf, np.inf, np.inf])
    all_max = np.array([-np.inf, -np.inf, -np.inf])
    for index, raw in enumerate(raw_parts):
        verts = raw["verts"] * mm_scale
        faces = raw["faces"]
        part_volume = _mesh_volume_m3(verts, faces)
        parts.append({
            "name": raw["name"],
            "verts": verts.tolist(),
            "tris": faces.tolist(),
            "color": PALETTE[index % len(PALETTE)],
            "material": raw["material"],
            "volumeM3": part_volume,
        })
        total_volume += part_volume
        all_min = np.minimum(all_min, verts.min(axis=0))
        all_max = np.maximum(all_max, verts.max(axis=0))

    bounds = {
        "x": [round(float(all_min[0]), 3), round(float(all_max[0]), 3)],
        "y": [round(float(all_min[1]), 3), round(float(all_max[1]), 3)],
        "z": [round(float(all_min[2]), 3), round(float(all_max[2]), 3)],
    }
    with open(ifc_path, "rb") as fh:
        sha = hashlib.sha256(fh.read()).hexdigest()

    stage_dir = os.path.join(os.path.dirname(output_rcif), f".ifc_stage_{uuid.uuid4().hex[:8]}")
    os.makedirs(os.path.join(stage_dir, "display"), exist_ok=True)
    try:
        glb_writer.write_glb_parts(parts, os.path.join(stage_dir, "display", "preview.glb"))

        display_name = name or os.path.splitext(os.path.basename(ifc_path))[0]
        component = {
            "format": "cbf-component",
            "schemaVersion": "1.4.2",
            "identity": {
                "componentId": str(uuid.uuid5(uuid.NAMESPACE_URL, f"rcif:ifc:{sha}:{family_id}:{revision}")),
                "familyId": family_id,
                "revision": revision,
                "name": display_name,
                "category": "ImportedIFC",
                "semanticCode": "Bridge.ImportedIFC",
                "sourceMode": "mesh-only",
                "placement": {"modes": ["point"], "orientation": "azimuth"},
                "drawingCode": family_id,
            },
            "environment": {
                "units": {"length": "mm", "angle": "rad"},
                "tolerance": {"linear": 0.01, "angular": 0.001},
            },
            "parameters": {
                "schema": [
                    {"name": "PartCount", "label": "命名部位数", "type": "int", "default": len(parts), "access": "out"},
                    {"name": "NetVolume", "label": "网格净方量", "type": "real", "default": round(total_volume, 3), "unit": "m3", "access": "out"},
                ],
                "presets": [{
                    "modelNumber": "IMPORTED-IFC",
                    "label": display_name,
                    "drawingCode": family_id,
                    "params": {"PartCount": len(parts), "NetVolume": round(total_volume, 3)},
                    "isPrebaked": True,
                }],
            },
            "geometry": {
                "exactBrep": False,
                "defaultFormat": "glb",
                "defaultFile": "display/preview.glb",
                "bounds": bounds,
                "volumeM3": round(total_volume, 3),
                "triangles": int(sum(len(p["tris"]) for p in parts)),
            },
            "shapes": {
                part["name"]: {
                    "label": part["name"],
                    "code": f"Bridge.ImportedIFC.{part['name'].replace(' ', '_')}",
                    "geomType": "shell",
                    "material": part["material"],
                    "node": part["name"],
                    "volumeM3": part.get("volumeM3"),
                }
                for part in parts
            },
            "qa": {
                "source": "ifc",
                "ifcSchema": getattr(ifc, "schema", "IFC4"),
                "productCount": len(parts),
                "volumeMethod": "signed-tetrahedra",
            },
            "changelog": [{
                "version": revision,
                "date": __import__("datetime").date.today().isoformat(),
                "author": "RCIF-DSH-Studio",
                "type": "initial",
                "changes": [
                    f"由 {os.path.basename(ifc_path)} 转换（mesh-only）",
                    f"提取 {len(parts)} 个 IFC 命名部位，网格体积 {total_volume:.3f} m³",
                ],
            }],
        }
        pkg_path, pkg_sha = rcifc.build_rcif_package(stage_dir, output_rcif, component)
        return pkg_path, {
            "packageSha256": pkg_sha,
            "partCount": len(parts),
            "volumeM3": round(total_volume, 3),
            "bounds": bounds,
        }
    finally:
        shutil.rmtree(stage_dir, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="IFC -> RCIF mesh-only 转换器")
    parser.add_argument("ifc_path", help="输入 IFC 文件")
    parser.add_argument("output", help="输出 .rcif 路径或目录")
    parser.add_argument("--family-id", default="ImportedIFC")
    parser.add_argument("--revision", default="1.0.0")
    parser.add_argument("--name", default=None)
    args = parser.parse_args()
    try:
        pkg, info = convert_ifc_to_rcif(
            args.ifc_path, args.output, args.family_id, args.revision, args.name
        )
        print(f"[OK] 生成成功: {pkg} (SHA256={info['packageSha256'][:16]}..., parts={info['partCount']})")
        return 0
    except Exception as exc:  # noqa: BLE001 - CLI 边界
        print(f"[ERROR] IFC 转换失败: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
