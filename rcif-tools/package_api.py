"""RCIF 构件包只读检查（供 FastAPI 调试后端与测试复用）。

与 `dsh-rcif/dist/lib/package.js` 保持同一套字段契约：
metadata / files / checksums / preview / source / provenance。
"""

from __future__ import annotations

import hashlib
import json
import os
import zipfile
from typing import Any, Dict, List, Optional

MAX_ARCHIVE_BYTES = 128 * 1024 * 1024
MAX_ENTRIES = 500
MAX_FILE_BYTES = 100 * 1024 * 1024
MAX_SOURCE_BYTES = 2 * 1024 * 1024

_CACHE: Dict[str, tuple[float, int, Dict[str, Any]]] = {}


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _entry_name(name: str) -> str:
    normalized = name.replace("\\", "/")
    while normalized.startswith("./"):
        normalized = normalized[2:]
    return normalized


def _find(entries: Dict[str, zipfile.ZipInfo], candidates: List[Optional[str]]) -> Optional[str]:
    for candidate in candidates:
        if not candidate:
            continue
        name = _entry_name(str(candidate))
        if name in entries:
            return name
    return None


def read_package(package_path: str) -> Dict[str, Any]:
    resolved = os.path.realpath(package_path)
    stat = os.stat(resolved)
    if stat.st_size > MAX_ARCHIVE_BYTES:
        raise ValueError("构件包超过 128MB 上限")
    key = f"{resolved}|{stat.st_mtime}|{stat.st_size}"
    cached = _CACHE.get(key)
    if cached:
        return cached[2]

    with zipfile.ZipFile(resolved) as archive:
        infos = archive.infolist()
        if len(infos) > MAX_ENTRIES:
            raise ValueError(f"构件包条目数 {len(infos)} 超过 {MAX_ENTRIES} 上限")
        entries: Dict[str, zipfile.ZipInfo] = {}
        for info in infos:
            name = _entry_name(info.filename)
            if name.startswith("/") or ".." in name.split("/") or (len(name) > 1 and name[1] == ":"):
                raise ValueError(f"非法 ZIP 条目: {name}")
            if info.file_size > MAX_FILE_BYTES:
                raise ValueError(f"条目 {name} 超过 100MB 上限")
            entries[name] = info

        component_name = _find(entries, ["component.json"])
        if not component_name:
            raise ValueError("构件包缺少 component.json")
        with archive.open(component_name) as fh:
            component = json.loads(fh.read().decode("utf-8"))

        checksums: Dict[str, Any] = {
            "present": False, "verified": False, "fileCount": 0, "checked": 0, "mismatches": []
        }
        checksums_name = _find(entries, ["checksums.json"])
        if checksums_name:
            try:
                with archive.open(checksums_name) as fh:
                    doc = json.loads(fh.read().decode("utf-8"))
                files = doc.get("files") if isinstance(doc, dict) else None
                if isinstance(files, dict):
                    mismatches = []
                    checked = 0
                    for name, expected in files.items():
                        if name not in entries:
                            mismatches.append(f"清单文件缺失: {name}")
                            continue
                        with archive.open(name) as fh:
                            actual = _sha256(fh.read())
                        checked += 1
                        if actual != expected:
                            mismatches.append(f"SHA-256 失配: {name}")
                    checksums = {
                        "present": True,
                        "algorithm": doc.get("algorithm", "sha256"),
                        "verified": not mismatches,
                        "fileCount": len(files),
                        "checked": checked,
                        "mismatches": mismatches[:20],
                    }
                else:
                    checksums.update(present=True, mismatches=["checksums.json 缺少 files 映射"])
            except Exception as exc:  # noqa: BLE001
                checksums.update(present=True, mismatches=[f"checksums.json 解析失败: {exc}"])

        source_candidates = [component.get("recipe", {}).get("source"), "generators/generator.py", "generator.py"]
        source_name = _find(entries, source_candidates)
        source = {
            "available": bool(source_name),
            "path": source_name,
            "size": entries[source_name].file_size if source_name else 0,
        }

        preview_candidates = ["display/preview.glb", "rcif/display/preview.glb"]
        default_file = (component.get("geometry") or {}).get("defaultFile")
        if isinstance(default_file, str) and default_file.lower().endswith(".glb"):
            preview_candidates.append(default_file)
        preview_name = _find(entries, preview_candidates)
        preview = {
            "available": bool(preview_name),
            "path": preview_name,
            "size": entries[preview_name].file_size if preview_name else 0,
        }

        files: List[Dict[str, Any]] = [
            {"path": name, "size": info.file_size, "compressedSize": info.compress_size}
            for name, info in entries.items()
        ]

        identity = component.get("identity") or {}
        geometry = component.get("geometry") or {}
        parameters = component.get("parameters") or {}
        shapes = component.get("shapes") or {}
        with open(resolved, "rb") as fh:
            package_sha = _sha256(fh.read())
        metadata = {
            "format": component.get("format"),
            "schemaVersion": component.get("schemaVersion"),
            "familyId": identity.get("familyId"),
            "componentId": identity.get("componentId"),
            "revision": identity.get("revision"),
            "name": identity.get("name") or identity.get("familyId"),
            "category": identity.get("category"),
            "semanticCode": identity.get("semanticCode"),
            "drawingCode": identity.get("drawingCode"),
            "sourceMode": identity.get("sourceMode"),
            "engine": (component.get("recipe") or {}).get("engine"),
            "recipeSource": (component.get("recipe") or {}).get("source"),
            "exactBrep": geometry.get("exactBrep") is True,
            "defaultFormat": geometry.get("defaultFormat"),
            "bounds": geometry.get("bounds"),
            "volumeM3": geometry.get("volumeM3"),
            "surfaceM2": geometry.get("surfaceM2"),
            "parameterCount": len(parameters.get("schema") or []),
            "presetCount": len(parameters.get("presets") or []),
            "shapeCount": len(shapes),
            "shapes": [
                {
                    "key": key,
                    "label": (value or {}).get("label") or key,
                    "code": (value or {}).get("code"),
                    "geomType": (value or {}).get("geomType"),
                    "material": (value or {}).get("material"),
                    "node": (value or {}).get("node") or key,
                    "volumeM3": (value or {}).get("volumeM3"),
                }
                for key, value in shapes.items()
            ],
            "fileCount": len(entries),
            "packageSha256": package_sha,
            "sizeBytes": stat.st_size,
            "checksums": checksums,
            "preview": preview,
            "source": source,
            "qa": component.get("qa"),
            "changelog": (component.get("changelog") or [])[-5:],
            "extractionMode": (component.get("qa") or {}).get("extractionMode"),
        }

        provenance = None
        prov_path = os.path.splitext(resolved)[0] + ".provenance.json"
        if os.path.isfile(prov_path):
            try:
                with open(prov_path, "r", encoding="utf-8") as fh:
                    provenance = json.load(fh)
            except Exception:  # noqa: BLE001
                provenance = None

        result = {
            "ok": True,
            "packagePath": resolved,
            "packageSha256": metadata["packageSha256"],
            "sizeBytes": stat.st_size,
            "updatedAt": __import__("datetime").datetime.fromtimestamp(stat.st_mtime).isoformat(),
            "component": component,
            "metadata": metadata,
            "files": files,
            "preview": preview,
            "source": source,
            "checksums": checksums,
            "provenance": provenance,
        }
    _CACHE[key] = (stat.st_mtime, stat.st_size, result)
    if len(_CACHE) > 32:
        _CACHE.pop(next(iter(_CACHE)))
    return result


def read_package_file(package_path: str, entry_name: str) -> bytes:
    resolved = os.path.realpath(package_path)
    name = _entry_name(entry_name)
    with zipfile.ZipFile(resolved) as archive:
        try:
            with archive.open(name) as fh:
                return fh.read(MAX_FILE_BYTES)
        except KeyError as exc:
            raise FileNotFoundError(f"构件包缺少 {name}") from exc


def read_package_source(package_path: str) -> Dict[str, Any]:
    meta = read_package(package_path)
    source = meta["source"]
    if not source["available"]:
        return {"ok": True, "available": False, "path": None, "text": ""}
    data = read_package_file(package_path, source["path"])
    text = data[:MAX_SOURCE_BYTES].decode("utf-8", "replace")
    return {
        "ok": True,
        "available": True,
        "path": source["path"],
        "text": text,
        "truncated": len(data) > MAX_SOURCE_BYTES,
    }
