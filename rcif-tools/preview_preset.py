"""按型号参数重建构件包预览 GLB（不生成新构件包）。

供 3090 插件 `/rcif-studio/api/preview` 调用：
  python preview_preset.py <package.rcif> --params '<json>' --out <preview.glb>

只允许参数化重建的构件包（generic_heuristic_draft / rfa_partatom_heuristic）；
精确 B-Rep（STEP/IFC）包没有可切换的参数化型号，直接拒绝。
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import tempfile
from collections import OrderedDict

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import generic_rebuild as builder  # noqa: E402
from glb_writer import write_glb_parts  # noqa: E402

ALLOWED_MODES = {"generic_heuristic_draft", "rfa_partatom_heuristic"}
COLORS = [(0.72, 0.71, 0.67), (0.58, 0.67, 0.80), (0.62, 0.72, 0.62)]


def merge_params(component: dict, overrides: dict) -> dict:
    params: dict = {}
    for item in component.get("parameters", {}).get("schema", []):
        if item.get("access") != "out" and "default" in item:
            params[item.get("name")] = item["default"]
    presets = component.get("parameters", {}).get("presets") or []
    if presets:
        params.update(presets[0].get("params") or {})
    for key, value in (overrides or {}).items():
        if isinstance(value, (int, float)) or (isinstance(value, str) and value.strip()):
            params[key] = value
    return params


def build_preview(package_path: str, overrides: dict, out_path: str, workdir: str | None = None) -> dict:
    work = os.path.abspath(workdir) if workdir else tempfile.mkdtemp(prefix="rcif_preview_")
    created = workdir is None
    if not created:
        os.makedirs(work, exist_ok=True)
    try:
        builder._safe_extract(package_path, work)
        with open(os.path.join(work, "component.json"), encoding="utf-8") as handle:
            component = json.load(handle)
        if component.get("qa", {}).get("extractionMode") not in ALLOWED_MODES:
            raise ValueError("该构件包为精确几何，没有可切换的参数化型号")
        generator = os.path.join(work, "generators", "generator.py")
        if not os.path.isfile(generator):
            raise FileNotFoundError("包内缺少 generators/generator.py")
        params = merge_params(component, overrides)
        built = builder._execute_generator(generator, params) or {}
        if not built:
            raise RuntimeError("生成器返回空部件集合")
        parts = OrderedDict((key, builder._shape(shape)) for key, shape in built.items())
        shape_meta = component.get("shapes") or {}
        items = []
        bounds = {"x": [float("inf"), float("-inf")], "y": [float("inf"), float("-inf")], "z": [float("inf"), float("-inf")]}
        total = 0.0
        for index, (key, shape) in enumerate(parts.items()):
            if not hasattr(shape, "Volume"):
                raise TypeError(f"部件 {key} 不是 CadQuery Shape")
            box = shape.BoundingBox()
            bounds["x"] = [min(bounds["x"][0], box.xmin), max(bounds["x"][1], box.xmax)]
            bounds["y"] = [min(bounds["y"][0], box.ymin), max(bounds["y"][1], box.ymax)]
            bounds["z"] = [min(bounds["z"][0], box.zmin), max(bounds["z"][1], box.zmax)]
            raw = shape.Volume() / 1e9
            volume = round(raw, 6) if raw < 0.01 else round(raw, 3)
            total += volume
            meta = shape_meta.get(key) or {}
            material = meta.get("material") or "C35"
            label = meta.get("label") or key
            code = meta.get("code") or f"Preview.{key}"
            verts, tris = shape.tessellate(2.0, 0.25)
            items.append({
                "name": key,
                "verts": [(v.x, v.y, v.z) for v in verts],
                "tris": [tuple(t) for t in tris],
                "color": COLORS[index % len(COLORS)],
                "material": material,
                "extras": {
                    "shapeKey": key,
                    "instanceTag": key,
                    "material": material,
                    "label": label,
                    "code": code,
                    "volumeM3": volume,
                },
            })
        if not items:
            raise RuntimeError("没有可导出的部件")
        write_glb_parts(items, out_path)
        return {
            "shapeCount": len(items),
            "volumeM3": round(total, 6) if total < 0.01 else round(total, 3),
            "bounds": bounds,
            "params": params,
        }
    finally:
        if created:
            shutil.rmtree(work, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("package")
    parser.add_argument("--params", default="{}")
    parser.add_argument("--out", required=True)
    parser.add_argument("--workdir", default=None)
    args = parser.parse_args()
    try:
        overrides = json.loads(args.params or "{}")
        if not isinstance(overrides, dict):
            raise ValueError("params 必须是对象")
        meta = build_preview(
            os.path.abspath(args.package),
            overrides,
            os.path.abspath(args.out),
            args.workdir,
        )
        print(json.dumps({"ok": True, **meta}, ensure_ascii=False))
        return 0
    except Exception as error:  # noqa: BLE001
        print(json.dumps({"ok": False, "error": str(error)}, ensure_ascii=False))
        return 2


if __name__ == "__main__":
    sys.exit(main())
