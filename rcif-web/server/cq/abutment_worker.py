"""CadQuery bridge for the web mock.

Reads {"params": {...}} from stdin, builds the StraightAbutment parts with the
existing CadQuery generator, tessellates them and writes a JSON mesh payload.
The Node server converts this payload into the JHB frame consumed by CadViewport.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
import importlib.util

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

DEFAULT_GENERATOR = HERE / "generator.py"

# Web RCIF shape slots (the package groups left/right instances into one shape)
SLOT_BY_PART = {
    "Footing": 0,
    "Body": 1,
    "Cap": 2,
    "ParapetWall": 3,
    "TopTray": 4,
    "BearingPadLeft": 5,
    "BearingPadRight": 5,
    "WaterCurbA": 6,
    "WaterCurbBLeft": 6,
    "WaterCurbBRight": 6,
}
SHAPE_KEYS = [
    "Footing",
    "Body",
    "Cap",
    "ParapetWall",
    "TopTray",
    "BearingPad",
    "WaterCurb",
    "BearingPadTopFace",
]


def load_generator(path: str):
    generator_path = Path(path) if path else DEFAULT_GENERATOR
    spec = importlib.util.spec_from_file_location("uploaded_cq_generator", generator_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"无法加载生成器: {generator_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def build_payload(payload: dict) -> dict:
    params = payload.get("params") or {}
    generator = load_generator(str(payload.get("generatorPath") or ""))
    shape_keys = payload.get("shapeKeys") or SHAPE_KEYS
    slot_by_part = payload.get("partSlots") or SLOT_BY_PART
    parts = generator.build_parts(params)

    positions: list[float] = []
    indices: list[int] = []
    face_ids: list[int] = []
    face_to_shape: list[int] = []
    face_groups: list[dict] = []
    bounds_min = [float("inf")] * 3
    bounds_max = [float("-inf")] * 3
    volume_mm3 = 0.0
    area_mm2 = 0.0

    for part_key, shape in parts.items():
        if shape is None:
            continue
        slot = slot_by_part.get(part_key)
        if slot is None and part_key in shape_keys:
            slot = shape_keys.index(part_key)
        if slot is None:
            continue
        vertices, triangles = shape.tessellate(0.5, 0.25)
        base = len(positions) // 3
        for vertex in vertices:
            positions.extend([float(vertex.x), float(vertex.y), float(vertex.z)])
            for axis in range(3):
                bounds_min[axis] = min(bounds_min[axis], float(getattr(vertex, "xyz"[axis])))
                bounds_max[axis] = max(bounds_max[axis], float(getattr(vertex, "xyz"[axis])))
        for triangle in triangles:
            face_id = len(face_ids)
            face_ids.append(face_id)
            face_to_shape.append(slot)
            indices.extend([base + int(triangle[0]), base + int(triangle[1]), base + int(triangle[2])])
            face_groups.append({"faceId": face_id, "shapeIndex": slot, "areaMm2": 0.0})
        volume_mm3 += float(shape.Volume())
        area_mm2 += float(shape.Area())

    if not positions:
        raise RuntimeError("CadQuery returned empty abutment geometry")

    # Smooth vertex normals from triangle winding.
    normals = [0.0] * len(positions)
    for t in range(0, len(indices), 3):
        ia, ib, ic = indices[t], indices[t + 1], indices[t + 2]
        ax, ay, az = positions[ia * 3 : ia * 3 + 3]
        bx, by, bz = positions[ib * 3 : ib * 3 + 3]
        cx, cy, cz = positions[ic * 3 : ic * 3 + 3]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx - ax, cy - ay, cz - az
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        for index in (ia, ib, ic):
            normals[index * 3] += nx
            normals[index * 3 + 1] += ny
            normals[index * 3 + 2] += nz
    for i in range(0, len(normals), 3):
        length = (normals[i] ** 2 + normals[i + 1] ** 2 + normals[i + 2] ** 2) ** 0.5 or 1.0
        normals[i] /= length
        normals[i + 1] /= length
        normals[i + 2] /= length

    result = {
        "shapeKeys": shape_keys,
        "positions": positions,
        "indices": indices,
        "normals": normals,
        "faceIds": face_ids,
        "faceGroups": face_groups,
        "faceToShape": face_to_shape,
        "bounds": {"min": bounds_min, "max": bounds_max},
        "volumeMm3": volume_mm3,
        "areaMm2": area_mm2,
        "vertices": len(positions) // 3,
        "triangles": len(indices) // 3,
        "facesCount": len(shape_keys),
    }
    return result


def main() -> int:
    payload = json.loads(sys.stdin.read() or "{}")
    sys.stdout.write(json.dumps(build_payload(payload), ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
