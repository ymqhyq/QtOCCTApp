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
import builtins
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
from verify_sandbox import check_gate1_ast


_SANDBOX_ACTIVE = False
_SAFE_IMPORTS = {
    "__future__", "cadquery", "cq", "math", "typing", "collections",
    "dataclasses", "enum", "glb_writer", "build123d", "numpy", "np", "json",
    "ocp_vscode", "OCP",
}
_SAFE_BUILTIN_NAMES = {
    "abs", "all", "any", "bool", "dict", "enumerate", "float", "int",
    "isinstance", "len", "list", "max", "min", "pow", "range", "round", "set",
    "sorted", "str", "sum", "tuple", "zip", "object", "super", "property",
    "classmethod", "staticmethod", "__build_class__", "ValueError", "RuntimeError",
    "KeyError", "Exception", "TypeError", "AttributeError", "IndexError",
    "StopIteration", "True", "False", "None", "NotImplemented",
}


def _sandbox_audit(event, args):
    """Block write/shell/network syscalls only while generator code runs."""
    if not _SANDBOX_ACTIVE:
        return
    if event == "open":
        mode = args[1] if len(args) > 1 else ""
        flags = args[2] if len(args) > 2 else 0
        if mode and any(ch in str(mode) for ch in "wax+"):
            raise PermissionError(f"[SANDBOX] 禁止生成器写文件: {args[0]}")
        if isinstance(flags, int):
            write_flags = os.O_WRONLY | os.O_RDWR | getattr(os, "O_CREAT", 0) | getattr(os, "O_TRUNC", 0)
            if flags & write_flags:
                raise PermissionError(f"[SANDBOX] 禁止生成器底层写文件: {args[0]}")
    elif event.startswith(("os.system", "os.exec", "subprocess.", "socket.", "shutil.", "posix.", "winreg.", "ctypes.")):
        raise PermissionError(f"[SANDBOX] 禁止生成器调用系统操作: {event}")


sys.addaudithook(_sandbox_audit)


def _safe_import(name, globals=None, locals=None, fromlist=(), level=0):
    root = name.split(".")[0]
    if root not in _SAFE_IMPORTS:
        raise ImportError(f"[SANDBOX] 禁止导入非白名单模块: {name}")
    return builtins.__import__(name, globals, locals, fromlist, level)


def _safe_builtins():
    values = {name: getattr(builtins, name) for name in _SAFE_BUILTIN_NAMES if hasattr(builtins, name)}
    values["__import__"] = _safe_import
    return values


def _install_export_blocks():
    """Temporarily replace CQ export entry points while generator code runs."""
    restores = []

    def deny(*args, **kwargs):
        raise PermissionError("[SANDBOX] 生成器禁止导出/写盘")

    def patch(obj, names):
        for name in names:
            if obj is not None and hasattr(obj, name):
                original = getattr(obj, name)
                restores.append((obj, name, original))
                setattr(obj, name, deny)

    if hasattr(cq, "exporters"):
        patch(cq.exporters, ("export", "exportShape", "exportSTEP", "exportSTL", "exportAMF", "exportSVG", "exportDXF"))
    try:
        from cadquery.occ_impl.shapes import Shape
        patch(Shape, ("exportStep", "exportStl", "exportBrep", "exportShape"))
    except Exception:
        pass
    if hasattr(cq, "Workplane"):
        patch(cq.Workplane, ("exportStep", "exportStl"))

    def restore():
        for obj, name, original in reversed(restores):
            try:
                setattr(obj, name, original)
            except Exception:
                pass

    return restore


def _load_generator(path: str):
    spec = importlib.util.spec_from_file_location(f"generic_gen_{uuid.uuid4().hex[:8]}", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"无法加载生成器: {path}")
    module = importlib.util.module_from_spec(spec)
    module.__dict__["__builtins__"] = _safe_builtins()
    spec.loader.exec_module(module)
    if not hasattr(module, "build_parts"):
        raise RuntimeError("生成器缺少 build_parts(params) 函数")
    return module


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


def _execute_generator(path: str, params: dict):
    gate = check_gate1_ast(path)
    if not gate.passed:
        violations = "；".join((gate.details or {}).get("violations", [])[:5])
        raise ValueError(f"GATE-01 安全审查未通过: {gate.message}{('：' + violations) if violations else ''}")

    global _SANDBOX_ACTIVE
    previous_bytecode = sys.dont_write_bytecode
    restore_exports = _install_export_blocks()
    sys.dont_write_bytecode = True
    _SANDBOX_ACTIVE = True
    try:
        module = _load_generator(path)
        return module.build_parts(params) or {}
    finally:
        _SANDBOX_ACTIVE = False
        sys.dont_write_bytecode = previous_bytecode
        restore_exports()


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
        if component.get("qa", {}).get("extractionMode") not in ("generic_heuristic_draft", "rfa_partatom_heuristic"):
            raise ValueError("只允许重建 generic_heuristic_draft 通用草稿或 rfa_partatom_heuristic 构件包")
        generator_path = os.path.join(work, "generators", "generator.py")
        if source_text:
            os.makedirs(os.path.dirname(generator_path), exist_ok=True)
            with open(generator_path, "w", encoding="utf-8") as handle:
                handle.write(source_text)
        if not os.path.isfile(generator_path):
            raise FileNotFoundError("包内缺少 generators/generator.py")

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

        built = _execute_generator(generator_path, next_params) or {}
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
        bounds = {"x": [float("inf"), float("-inf")], "y": [float("inf"), float("-inf")], "z": [float("inf"), float("-inf")]}
        shape_meta = OrderedDict()
        total_volume = 0.0
        for index, (key, shape) in enumerate(parts.items()):
            bb = shape.BoundingBox()
            bounds["x"] = [min(bounds["x"][0], bb.xmin), max(bounds["x"][1], bb.xmax)]
            bounds["y"] = [min(bounds["y"][0], bb.ymin), max(bounds["y"][1], bb.ymax)]
            bounds["z"] = [min(bounds["z"][0], bb.zmin), max(bounds["z"][1], bb.zmax)]
            raw_vol = shape.Volume() / 1e9
            volume = round(raw_vol, 6) if raw_vol < 0.01 else round(raw_vol, 3)
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
            verts, tris = shape.tessellate(1.0, 0.08)
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

        vol_rounded = round(total_volume, 6) if total_volume < 0.01 else round(total_volume, 3)
        next_params["NetVolume"] = vol_rounded

        next_revision = revision or component.get("identity", {}).get("revision") or "0.1.0"
        component.setdefault("identity", {})["revision"] = next_revision
        with open(generator_path, "rb") as handle:
            digest = hashlib.sha256(handle.read()).hexdigest()
        cache_key = rcifc.compute_cache_key(digest, next_params)
        component.setdefault("geometry", {}).update({
            "exactBrep": True,
            "defaultFormat": "step",
            "file": "geometry/default.step",
            "defaultFile": "geometry/default.step",
            "display": "display/preview.glb",
            "bounds": bounds,
            "volumeM3": vol_rounded,
            "defaultCacheKey": cache_key,
        })
        component["shapes"] = shape_meta
        component.setdefault("parameters", {})
        for item in component["parameters"].get("schema", []):
            if item.get("name") in next_params and item.get("access") != "out":
                item["default"] = next_params[item["name"]]
        existing_presets = component["parameters"].get("presets") or [{}]
        preset = {
            **existing_presets[0],
            "params": next_params,
            "isPrebaked": True,
            "cacheKey": cache_key,
        }
        component["parameters"]["presets"] = [preset] + [
            {**item, "isPrebaked": False} for item in existing_presets[1:]
        ]
        component.setdefault("recipe", {}).update({"digest": digest, "source": "generators/generator.py", "entry": "build_parts"})
        qa = component.setdefault("qa", {})
        qa.pop("packageSha256", None)
        previous_rebuilds = qa.get("rebuilds") or []
        qa["rebuilds"] = (previous_rebuilds + [{
            "at": datetime.now(timezone.utc).isoformat(),
            "params": next_params,
            "sourceChanged": bool(source_text),
            "note": note,
            "revision": next_revision,
            "volumeM3": vol_rounded,
        }])[-20:]
        component.setdefault("changelog", []).append({
            "version": next_revision,
            "date": datetime.now(timezone.utc).date().isoformat(),
            "author": "RCIF-DSH-Studio",
            "type": "rebuild",
            "changes": [f"参数化重建：{note}", f"体积 {vol_rounded} m³"],
        })
        out_path = os.path.abspath(out_path or package_path)
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        pkg, pkg_sha = rcifc.build_rcif_package(work, out_path, component)
        return pkg, {
            "packagePath": pkg,
            "packageSha256": pkg_sha,
            "revision": next_revision,
            "volumeM3": vol_rounded,
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
