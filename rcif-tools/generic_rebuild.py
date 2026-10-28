"""Rebuild a generic PDF draft from an existing .rcif package.

Inputs are the package itself plus optional parameter overrides and/or a full
replacement ``generators/generator.py``.  The package is extracted into a
temporary directory, the generator is executed, STEP/GLB/component metadata are
regenerated, and a new deterministic .rcif is written.  This is intentionally
separate from the specialized abutment/girder gate pipeline: a generic massing
draft only promises a valid B-Rep, preview and checksums, not bridge-specific
bearing-pad/volume rules.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import shutil
import sys
import tempfile
import uuid
import zipfile
from collections import OrderedDict
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import cadquery as cq

import rcifc
from glb_writer import write_glb_parts


def _safe_extract(package_path: str, target: str) -> None:
    with zipfile.ZipFile(package_path) as archive:
        for info in archive.infolist():
            name = info.filename.replace("\\", "/")
            if name.startswith("/") or ".." in name.split("/"):
                raise ValueError(f"包内路径越界: {info.filename}")
            destination = os.path.realpath(os.path.join(target, name))
            if not destination.startswith(os.path.realpath(target) + os.sep):
                raise ValueError(f"包内路径越界: {info.filename}")
            archive.extract(info, target)


def _load_generator(path: str):
    spec = importlib.util.spec_from_file_location(f"generic_gen_{uuid.uuid4().hex[:8]}", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"无法加载生成器: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if not hasattr(module, "build_parts"):
        raise RuntimeError("生成器缺少 build_parts(params) 函数")
    return module


def _shape(value):
    return value.val() if hasattr(value, "val") and not hasattr(value, "Volume") else value


def rebuild(package_path: str, out_path: str | None = None, params: dict | None = None,
            source_text: str | None = None, revision: str | None = None,
            note: str = "工作台/助手重建") -> tuple[str, dict]:
    package_path = os.path.abspath(package_path)
    if not os.path.isfile(package_path):
        raise FileNotFoundError(f"构件包不存在: {package_path}")
    work = tempfile.mkdtemp(prefix="rcif_generic_rebuild_")
    try:
        _safe_extract(package_path, work)
        component_path = os.path.join(work, "component.json")
        if not os.path.isfile(component_path):
            raise ValueError("不是有效 .rcif：缺少 component.json")
        with open(component_path, encoding="utf-8") as handle:
            component = json.load(handle)
        if component.get("qa", {}).get("extractionMode") != "generic_heuristic_draft":
            raise ValueError("只允许重建 generic_heuristic_draft 通用草稿")
        generator_path = os.path.join(work, "generators", "generator.py")
        if source_text:
            os.makedirs(os.path.dirname(generator_path), exist_ok=True)
            with open(generator_path, "w", encoding="utf-8") as handle:
                handle.write(source_text)
        if not os.path.isfile(generator_path):
            raise FileNotFoundError("包内缺少 generators/generator.py")
        generator = _load_generator(generator_path)

        preset = (component.get("parameters", {}).get("presets") or [{}])[0]
        defaults = dict(preset.get("params") or {})
        for item in component.get("parameters", {}).get("schema", []):
            if item.get("access") != "out" and "default" in item:
                defaults.setdefault(item.get("name"), item["default"])
        next_params = dict(defaults)
        for key, value in (params or {}).items():
            try:
                next_params[key] = float(value) if not isinstance(value, str) else value
            except (TypeError, ValueError):
                next_params[key] = value

        built = generator.build_parts(next_params) or {}
        if not built:
            raise RuntimeError("生成器返回空部件集合")
        parts = OrderedDict((key, _shape(shape)) for key, shape in built.items())
        for key, shape in parts.items():
            if not hasattr(shape, "Volume"):
                raise TypeError(f"部件 {key} 不是 CadQuery Shape")

        # 重新导出几何。
        for name in ("geometry", "display"):
            os.makedirs(os.path.join(work, name), exist_ok=True)
        assembly = cq.Assembly()
        for key, shape in parts.items():
            assembly.add(shape, name=key)
        assembly.save(os.path.join(work, "geometry", "default.step"), exportType="STEP")

        glb_items = []
        colors = [(0.72, 0.71, 0.67), (0.58, 0.67, 0.80), (0.62, 0.72, 0.62)]
        bounds = {"x": [0.0, 0.0], "y": [0.0, 0.0], "z": [0.0, 0.0]}
        shape_meta = OrderedDict()
        total_volume = 0.0
        for index, (key, shape) in enumerate(parts.items()):
            bb = shape.BoundingBox()
            bounds["x"] = [min(bounds["x"][0], bb.xmin), max(bounds["x"][1], bb.xmax)]
            bounds["y"] = [min(bounds["y"][0], bb.ymin), max(bounds["y"][1], bb.ymax)]
            bounds["z"] = [min(bounds["z"][0], bb.zmin), max(bounds["z"][1], bb.zmax)]
            volume = round(shape.Volume() / 1e9, 3)
            total_volume += volume
            previous = (component.get("shapes") or {}).get(key) or {}
            shape_meta[key] = {
                **previous,
                "label": previous.get("label") or key,
                "code": previous.get("code") or f"Generic.{key}",
                "geomType": "solid",
                "material": previous.get("material") or "C35",
                "node": previous.get("node") or key,
                "volumeM3": volume,
            }
            verts, tris = shape.tessellate(5.0, 0.35)
            glb_items.append({
                "name": key,
                "verts": [(v.x, v.y, v.z) for v in verts],
                "tris": [tuple(t) for t in tris],
                "color": colors[index % len(colors)],
                "material": shape_meta[key]["material"],
                "extras": {
                    "shapeKey": key,
                    "instanceTag": key,
                    "material": shape_meta[key]["material"],
                    "label": shape_meta[key]["label"],
                    "code": shape_meta[key]["code"],
                    "volumeM3": volume,
                },
            })
        if not glb_items:
            raise RuntimeError("没有可导出的部件")
        write_glb_parts(glb_items, os.path.join(work, "display", "preview.glb"))

        next_params["NetVolume"] = round(total_volume, 3)

        next_revision = revision or component.get("identity", {}).get("revision") or "0.1.0"
        component.setdefault("identity", {})["revision"] = next_revision
        component.setdefault("geometry", {}).update({
            "exactBrep": True,
            "defaultFormat": "step",
            "file": "geometry/default.step",
            "defaultFile": "geometry/default.step",
            "display": "display/preview.glb",
            "bounds": bounds,
            "volumeM3": round(total_volume, 3),
            "defaultCacheKey": hashlib.sha256(json.dumps(next_params, sort_keys=True, ensure_ascii=False).encode("utf-8")).hexdigest(),
        })
        component["shapes"] = shape_meta
        component.setdefault("parameters", {})
        for item in component["parameters"].get("schema", []):
            if item.get("name") in next_params and item.get("access") != "out":
                item["default"] = next_params[item["name"]]
        preset = {
            **(component["parameters"].get("presets") or [{}])[0],
            "params": next_params,
            "isPrebaked": True,
            "cacheKey": component["geometry"]["defaultCacheKey"],
        }
        component["parameters"]["presets"] = [preset]
        with open(generator_path, "rb") as handle:
            digest = hashlib.sha256(handle.read()).hexdigest()
        component["geometry"]["defaultCacheKey"] = hashlib.sha256(
            json.dumps(next_params, sort_keys=True, ensure_ascii=False).encode("utf-8")
        ).hexdigest()
        component["parameters"]["presets"][0]["cacheKey"] = component["geometry"]["defaultCacheKey"]
        component.setdefault("recipe", {}).update({"digest": digest, "source": "generators/generator.py", "entry": "build_parts"})
        qa = component.setdefault("qa", {})
        previous_rebuilds = qa.get("rebuilds") or []
        qa["rebuilds"] = (previous_rebuilds + [{
            "at": datetime.now(timezone.utc).isoformat(),
            "params": next_params,
            "sourceChanged": bool(source_text),
            "note": note,
            "revision": next_revision,
            "volumeM3": round(total_volume, 3),
        }])[-20:]
        component.setdefault("changelog", []).append({
            "version": next_revision,
            "date": datetime.now(timezone.utc).date().isoformat(),
            "author": "RCIF-DSH-Studio",
            "type": "rebuild",
            "changes": [f"通用草稿重建：{note}", f"体积 {round(total_volume, 3)} m³"],
        })
        component["qa"]["packageSha256"] = ""

        out_path = os.path.abspath(out_path or package_path)
        if out_path == package_path:
            out_path = package_path
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        pkg, pkg_sha = rcifc.build_rcif_package(work, out_path, component)
        component["qa"]["packageSha256"] = pkg_sha
        with open(os.path.join(work, "component.json"), "w", encoding="utf-8") as handle:
            json.dump(component, handle, ensure_ascii=False, indent=2)
        pkg, pkg_sha = rcifc.build_rcif_package(work, out_path, component)
        return pkg, {
            "packagePath": pkg,
            "packageSha256": pkg_sha,
            "revision": next_revision,
            "volumeM3": round(total_volume, 3),
            "bounds": bounds,
            "shapeCount": len(parts),
            "params": next_params,
            "sourceChanged": bool(source_text),
        }
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="重建通用 PDF 草稿")
    parser.add_argument("package")
    parser.add_argument("--output", default=None)
    parser.add_argument("--params-json", default=None)
    parser.add_argument("--params-json-file", default=None)
    parser.add_argument("--source-file", default=None)
    parser.add_argument("--revision", default=None)
    parser.add_argument("--note", default="命令行重建")
    args = parser.parse_args()
    params = json.loads(args.params_json) if args.params_json else None
    if args.params_json_file:
        with open(args.params_json_file, encoding="utf-8") as handle:
            params = json.load(handle)
    source = open(args.source_file, encoding="utf-8").read() if args.source_file else None
    pkg, info = rebuild(args.package, args.output, params, source, args.revision, args.note)
    print(json.dumps({"ok": True, "packagePath": pkg, **info}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
