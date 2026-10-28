"""Unregistered PDF -> generic parametric RCIF draft.

This is the fallback for drawings that are not in ``drawings.py``.  It does
*not* claim to understand a drawing like a CAD kernel.  It extracts the text
layer, infers a component family from keywords and a handful of labelled
dimensions, then builds a reviewable massing model with a real generator.

The draft is deliberately marked ``generic_heuristic_draft`` and carries
confidence / missing-parameter metadata.  Engineers correct it through the
workbench (3D screenshot, script annotation, assistant conversation) and the
normal package compile/approval path.
"""

from __future__ import annotations

import csv
import hashlib
import json
import os
import re
import shutil
import uuid
from collections import OrderedDict
from datetime import date

import pymupdf

HERE = os.path.dirname(os.path.abspath(__file__))

import cadquery as cq

import rcifc
from glb_writer import write_glb_parts


TYPE_RULES = [
    ("pier", [r"桥墩", r"墩身", r"墩柱", r"\bpier\b"]),
    ("abutment", [r"桥台", r"台身", r"\babutment\b"]),
    ("girder", [r"箱梁", r"梁体", r"预制梁", r"\bgirder\b"]),
    ("culvert", [r"涵洞", r"涵身", r"\bculvert\b"]),
    ("wall", [r"挡土墙", r"挡墙", r"墙身", r"\bwall\b"]),
    ("pilecap", [r"承台", r"桩基承台", r"pile\s*cap"]),
    ("pile", [r"桩基", r"灌注桩", r"钻孔桩", r"\bpile\b"]),
]

DEFAULT_DIMS = {
    "pier": {"Length": 3000.0, "Width": 2000.0, "Height": 8000.0, "Diameter": 0.0, "WallThickness": 0.0},
    "abutment": {"Length": 5000.0, "Width": 8000.0, "Height": 6000.0, "Diameter": 0.0, "WallThickness": 0.0},
    "girder": {"Length": 31500.0, "Width": 12600.0, "Height": 3000.0, "Diameter": 0.0, "WallThickness": 0.0},
    "culvert": {"Length": 4000.0, "Width": 3000.0, "Height": 3000.0, "Diameter": 0.0, "WallThickness": 300.0},
    "wall": {"Length": 10000.0, "Width": 800.0, "Height": 5000.0, "Diameter": 0.0, "WallThickness": 0.0},
    "pilecap": {"Length": 6000.0, "Width": 6000.0, "Height": 2000.0, "Diameter": 0.0, "WallThickness": 0.0},
    "pile": {"Length": 1000.0, "Width": 1000.0, "Height": 10000.0, "Diameter": 1000.0, "WallThickness": 0.0},
    "generic": {"Length": 5000.0, "Width": 4000.0, "Height": 4000.0, "Diameter": 0.0, "WallThickness": 0.0},
}

LABELS = {
    "Length": ["总长", "长度", "桥长", "跨度", "顺桥向", "长", "length", "span"],
    "Width": ["总宽", "宽度", "桥面宽", "横向", "宽", "width"],
    "Height": ["总高", "高度", "梁高", "墩高", "台高", "高", "height"],
    "Thickness": ["壁厚", "厚度", "板厚", "thickness"],
    "Diameter": ["直径", "桩径", "半径", "diameter"],
}

TYPE_NAMES = {
    "pier": "桥墩",
    "abutment": "桥台",
    "girder": "梁体",
    "culvert": "涵洞",
    "wall": "挡墙",
    "pilecap": "承台",
    "pile": "桩",
    "generic": "通用构件",
}


def _sha256(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _detect_type(text: str, filename: str) -> tuple[str, list[str]]:
    haystack = f"{text}\n{filename}"
    for kind, patterns in TYPE_RULES:
        for pattern in patterns:
            if re.search(pattern, haystack, re.IGNORECASE):
                return kind, [pattern]
    return "generic", []


def _to_mm(value: float, unit: str) -> float:
    unit = (unit or "mm").lower()
    if unit == "m":
        return value * 1000.0
    if unit == "cm":
        return value * 10.0
    return value


def _extract_dimensions(text: str) -> tuple[dict, dict, list[str], list[str]]:
    values = {}
    evidence = {}
    labels = {k: v for k, v in LABELS.items()}
    for canonical, tokens in labels.items():
        for token in tokens:
            pattern = re.compile(
                rf"{re.escape(token)}[^\d]{{0,24}}([0-9]+(?:\.[0-9]+)?)\s*(mm|cm|m)?",
                re.IGNORECASE,
            )
            match = pattern.search(text)
            if not match:
                continue
            raw = float(match.group(1))
            unit = match.group(2) or "mm"
            value = _to_mm(raw, unit)
            if value <= 0:
                continue
            key = "Thickness" if canonical == "Thickness" else canonical
            if key == "Thickness":
                key = "WallThickness"
            values[key] = round(value, 1)
            evidence[key] = match.group(0).strip()
            break
    missing = [name for name in ("Length", "Width", "Height") if name not in values]
    warnings = []
    if missing:
        warnings.append("缺少明确标注的尺寸，已使用同类构件默认体量；请人工核对")
    if not text.strip():
        warnings.append("PDF 无文字层，未做 OCR；当前只能生成占位体量")
    return values, evidence, missing, warnings


def _box(length: float, width: float, height: float, z0: float = 0.0):
    return cq.Workplane("XY").box(length, width, height, centered=(True, True, False)).translate((0, 0, z0))


def _parts(kind: str, dims: dict):
    length = max(100.0, float(dims["Length"]))
    width = max(100.0, float(dims["Width"]))
    height = max(100.0, float(dims["Height"]))
    diameter = max(0.0, float(dims.get("Diameter") or 0.0))
    wall = max(50.0, float(dims.get("WallThickness") or 300.0))
    parts = OrderedDict()

    if kind == "pier":
        footing_h = min(800.0, height * 0.15)
        parts["Footing"] = _box(length * 1.4, width * 1.4, footing_h, 0.0)
        if diameter > 0:
            column = cq.Workplane("XY").circle(diameter / 2.0).extrude(height).translate((0, 0, footing_h))
        else:
            column = _box(length * 0.55, width * 0.55, height, footing_h)
        parts["Column"] = column
        parts["Cap"] = _box(length, width, max(300.0, height * 0.08), footing_h + height)
    elif kind == "abutment":
        footing_h = min(800.0, height * 0.15)
        parts["Footing"] = _box(length * 1.25, width * 1.25, footing_h, 0.0)
        parts["Body"] = _box(length, width, height, footing_h)
        parts["Cap"] = _box(length * 1.1, width * 1.05, max(300.0, height * 0.08), footing_h + height)
    elif kind == "girder":
        parts["Body"] = _box(length, width, height, 0.0)
    elif kind == "culvert":
        outer = _box(length, width, height, 0.0)
        inner = _box(max(10.0, length - 2 * wall), max(10.0, width - 2 * wall), height + 2, wall)
        parts["Body"] = outer.cut(inner)
    elif kind == "wall":
        parts["Footing"] = _box(length, width * 2.0, max(400.0, height * 0.1), 0.0)
        parts["Wall"] = _box(length, width, height, max(400.0, height * 0.1))
    elif kind == "pilecap":
        parts["Body"] = _box(length, width, height, 0.0)
    elif kind == "pile":
        parts["Pile"] = cq.Workplane("XY").circle(max(100.0, diameter or length) / 2.0).extrude(height)
    else:
        parts["Body"] = _box(length, width, height, 0.0)
    return OrderedDict((key, value.val() if hasattr(value, "val") else value) for key, value in parts.items())


GENERATOR_TEMPLATE = '''"""Auto-generated generic PDF component generator (draft).

Correct dimensions and shape logic after reviewing the 3D view / drawing,
then rebuild the package.  This script mirrors the massing used by the
initial draft; it is intentionally simple and readable.
"""

from collections import OrderedDict
import cadquery as cq

DEFAULT_PARAMS = __DEFAULT_PARAMS__

def _box(length, width, height, z0=0.0):
    return cq.Workplane("XY").box(length, width, height, centered=(True, True, False)).translate((0, 0, z0))

def build_parts(params=None):
    p = {**DEFAULT_PARAMS, **(params or {})}
    L = max(100.0, float(p["Length"]))
    W = max(100.0, float(p["Width"]))
    H = max(100.0, float(p["Height"]))
    D = max(0.0, float(p.get("Diameter", 0.0)))
    T = max(50.0, float(p.get("WallThickness", 300.0)))
    kind = p.get("ComponentType", "generic")
    parts = OrderedDict()
    if kind == "pier":
        fh = min(800.0, H * 0.15)
        parts["Footing"] = _box(L * 1.4, W * 1.4, fh, 0.0).val()
        if D > 0:
            parts["Column"] = cq.Workplane("XY").circle(D / 2.0).extrude(H).translate((0, 0, fh)).val()
        else:
            parts["Column"] = _box(L * 0.55, W * 0.55, H, fh).val()
        parts["Cap"] = _box(L, W, max(300.0, H * 0.08), fh + H).val()
    elif kind == "abutment":
        fh = min(800.0, H * 0.15)
        parts["Footing"] = _box(L * 1.25, W * 1.25, fh, 0.0).val()
        parts["Body"] = _box(L, W, H, fh).val()
        parts["Cap"] = _box(L * 1.1, W * 1.05, max(300.0, H * 0.08), fh + H).val()
    elif kind == "girder":
        parts["Body"] = _box(L, W, H, 0.0).val()
    elif kind == "culvert":
        outer = _box(L, W, H, 0.0)
        inner = _box(max(10.0, L - 2 * T), max(10.0, W - 2 * T), H + 2, T)
        parts["Body"] = outer.cut(inner).val()
    elif kind == "wall":
        fh = max(400.0, H * 0.1)
        parts["Footing"] = _box(L, W * 2.0, fh, 0.0).val()
        parts["Wall"] = _box(L, W, H, fh).val()
    elif kind == "pilecap":
        parts["Body"] = _box(L, W, H, 0.0).val()
    elif kind == "pile":
        parts["Pile"] = cq.Workplane("XY").circle(max(100.0, D or L) / 2.0).extrude(H).val()
    else:
        parts["Body"] = _box(L, W, H, 0.0).val()
    return parts

def build(params=None):
    return cq.Compound.makeCompound(list(build_parts(params).values()))
'''


def _component(kind, dims, params, name, family_id, revision, source_pdf, page_count, text_layer, confidence, missing, warnings, evidence, stage_dir):
    parts = _parts(kind, dims)
    bounds = {"x": [float("inf"), float("-inf")], "y": [float("inf"), float("-inf")], "z": [float("inf"), float("-inf")]}
    shape_meta = OrderedDict()
    total_volume = 0.0
    for key, shape in parts.items():
        bb = shape.BoundingBox()
        bounds["x"] = [min(bounds["x"][0], bb.xmin), max(bounds["x"][1], bb.xmax)]
        bounds["y"] = [min(bounds["y"][0], bb.ymin), max(bounds["y"][1], bb.ymax)]
        bounds["z"] = [min(bounds["z"][0], bb.zmin), max(bounds["z"][1], bb.zmax)]
        volume = round(shape.Volume() / 1e9, 3)
        total_volume += volume
        shape_meta[key] = {
            "label": f"{TYPE_NAMES.get(kind, '构件')} {key}",
            "code": f"Generic.{kind}.{key}",
            "geomType": "solid",
            "material": "C35",
            "node": key,
            "volumeM3": volume,
        }
    for axis in ("x", "y", "z"):
        bounds[axis] = [round(bounds[axis][0], 3), round(bounds[axis][1], 3)]
    schema = [
        {"name": "ModelNumber", "label": "草稿型号", "type": "enum", "default": f"{kind.upper()}-DRAFT"},
        {"name": "ComponentType", "label": "构件类型", "type": "enum", "default": kind},
        {"name": "Length", "label": "长度", "type": "real", "default": dims["Length"], "unit": "mm"},
        {"name": "Width", "label": "宽度", "type": "real", "default": dims["Width"], "unit": "mm"},
        {"name": "Height", "label": "高度", "type": "real", "default": dims["Height"], "unit": "mm"},
        {"name": "Diameter", "label": "直径", "type": "real", "default": dims.get("Diameter", 0.0), "unit": "mm"},
        {"name": "WallThickness", "label": "壁厚", "type": "real", "default": dims.get("WallThickness", 0.0), "unit": "mm"},
        {"name": "NetVolume", "label": "草稿体积", "type": "real", "default": round(total_volume, 3), "unit": "m3", "access": "out"},
    ]
    preset_params = {
        "ModelNumber": f"{kind.upper()}-DRAFT",
        "ComponentType": kind,
        **{k: dims.get(k, 0.0) for k in ("Length", "Width", "Height", "Diameter", "WallThickness")},
        "NetVolume": round(total_volume, 3),
    }
    with open(os.path.join(stage_dir, "generators", "generator.py"), "rb") as handle:
        generator_digest = hashlib.sha256(handle.read()).hexdigest()
    cache_key = rcifc.compute_cache_key(generator_digest, preset_params)
    component = {
        "format": "cbf-component",
        "schemaVersion": "1.4.2",
        "identity": {
            "componentId": str(uuid.uuid5(uuid.NAMESPACE_URL, f"rcif:generic-pdf:{source_pdf['sha256']}:{family_id}:{revision}")),
            "familyId": family_id,
            "revision": revision,
            "name": name,
            "category": "GenericDraft",
            "semanticCode": f"Generic.{kind}",
            "semanticProfileHint": "Railway-CRBIM-IFC4X3@1.4",
            "sourceMode": "parametric",
            "placement": {"modes": ["point"], "orientation": "azimuth"},
            "drawingCode": source_pdf["name"],
            "designSpeed": "-",
            "applicableSpan": [],
        },
        "description": f"由 {source_pdf['name']} 通用解析生成的可审阅体量草稿；类型={TYPE_NAMES.get(kind, kind)}，需工程师复核尺寸与剖面。",
        "environment": {"units": {"length": "mm", "angle": "rad", "mass": "t"}, "upAxis": "Z", "forwardAxis": "X", "tolerance": {"linear": 0.01, "angular": 0.001}},
        "parameters": {"schema": schema, "presets": [{"modelNumber": preset_params["ModelNumber"], "label": "通用草稿", "drawingCode": source_pdf["name"], "params": preset_params, "isPrebaked": True, "cacheKey": cache_key}]},
        "geometry": {"exactBrep": True, "defaultFormat": "step", "file": "geometry/default.step", "defaultFile": "geometry/default.step", "display": "display/preview.glb", "bounds": bounds, "volumeM3": round(total_volume, 3), "defaultCacheKey": cache_key},
        "shapes": shape_meta,
        "ports": [{"name": "Origin", "type": "reference", "position": [0.0, 0.0, 0.0]}],
        "recipe": {"engine": "cadquery", "source": "generators/generator.py", "entry": "build_parts", "version": "1.4.2", "digest": generator_digest, "inputs": ["ComponentType", "Length", "Width", "Height", "Diameter", "WallThickness"]},
        "qa": {
            "extractionMode": "generic_heuristic_draft",
            "pdfFingerprint": None,
            "textLayer": text_layer,
            "pageCount": page_count,
            "sourcePdfSha256": source_pdf["sha256"],
            "genericType": kind,
            "confidence": confidence,
            "missingParameters": missing,
            "warnings": warnings,
            "dimensionEvidence": evidence,
            "note": "通用草稿不是精确图纸识别；必须通过 3D 截图/脚本批注/助手多轮对话复核。",
        },
        "changelog": [{"version": revision, "date": date.today().isoformat(), "author": "RCIF-DSH-Studio", "type": "initial", "changes": ["通用 PDF 解析生成体量草稿", f"类型 {kind}，置信度 {confidence:.2f}"]}],
    }
    return component, parts


def convert_generic_pdf(pdf_path: str, output_rcif: str, family_id: str = "GenericDraft", revision: str = "0.1.0", name: str | None = None):
    pdf_path = os.path.abspath(pdf_path)
    output_rcif = os.path.abspath(output_rcif)
    if not output_rcif.lower().endswith(".rcif"):
        output_rcif = os.path.join(output_rcif, f"{family_id}_{revision}.rcif")
    if not os.path.isfile(pdf_path):
        raise FileNotFoundError(f"PDF 文件不存在: {pdf_path}")

    with pymupdf.open(pdf_path) as doc:
        page_count = len(doc)
        text = "\n".join(doc[i].get_text("text") for i in range(min(20, page_count)))
    filename = os.path.basename(pdf_path)
    kind, hits = _detect_type(text, filename)
    extracted, evidence, missing, warnings = _extract_dimensions(text)
    dims = {**DEFAULT_DIMS[kind], **extracted}
    found = len(extracted)
    confidence = min(0.9, 0.25 + (0.2 if hits else 0.0) + min(0.45, found * 0.11))
    if not text.strip():
        confidence = min(confidence, 0.25)
    resolved_name = name or f"{TYPE_NAMES.get(kind, '通用构件')}（PDF 草稿）"

    stage_dir = os.path.join(os.path.dirname(output_rcif), f".generic_pdf_stage_{uuid.uuid4().hex[:8]}")
    os.makedirs(os.path.join(stage_dir, "generators"), exist_ok=True)
    os.makedirs(os.path.join(stage_dir, "geometry"), exist_ok=True)
    os.makedirs(os.path.join(stage_dir, "display"), exist_ok=True)
    try:
        params_for_script = {k: dims.get(k, 0.0) for k in ("Length", "Width", "Height", "Diameter", "WallThickness")}
        params_for_script["ComponentType"] = kind
        with open(os.path.join(stage_dir, "generators", "generator.py"), "w", encoding="utf-8") as handle:
            handle.write(GENERATOR_TEMPLATE.replace(
                "__DEFAULT_PARAMS__",
                json.dumps(params_for_script, ensure_ascii=False, indent=2),
            ))

        parts = _parts(kind, dims)
        assembly = cq.Assembly()
        for key, shape in parts.items():
            assembly.add(shape, name=key)
        assembly.save(os.path.join(stage_dir, "geometry", "default.step"), exportType="STEP")
        glb_items = []
        colors = [(0.72, 0.71, 0.67), (0.58, 0.67, 0.80), (0.62, 0.72, 0.62)]
        for index, (key, shape) in enumerate(parts.items()):
            verts, tris = shape.tessellate(5.0, 0.35)
            glb_items.append({
                "name": key,
                "verts": [(v.x, v.y, v.z) for v in verts],
                "tris": [tuple(t) for t in tris],
                "color": colors[index % len(colors)],
                "material": "C35",
                "extras": {"shapeKey": key, "instanceTag": key, "material": "C35", "label": f"{TYPE_NAMES.get(kind, '构件')} {key}", "code": f"Generic.{kind}.{key}"},
            })
        if glb_items:
            write_glb_parts(glb_items, os.path.join(stage_dir, "display", "preview.glb"))

        # component needs the generated script digest, so build it after writing.
        component, _ = _component(kind, dims, params_for_script, resolved_name, family_id, revision,
                                  {"name": filename, "sha256": _sha256(pdf_path)}, page_count,
                                  bool(text.strip()), confidence, missing, warnings, evidence, stage_dir)
        pkg, pkg_sha = rcifc.build_rcif_package(stage_dir, output_rcif, component)
        return pkg, {
            "packageSha256": pkg_sha,
            "extractionMode": "generic_heuristic_draft",
            "genericType": kind,
            "confidence": confidence,
            "missingParameters": missing,
            "warnings": warnings,
            "pageCount": page_count,
            "textLayer": bool(text.strip()),
        }
    finally:
        shutil.rmtree(stage_dir, ignore_errors=True)
