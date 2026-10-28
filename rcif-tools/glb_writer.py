"""最小 glTF 2.0 (.glb) 写出器：支持多部位、每部位独立命名与材质配色。

每个部位 -> 一个 node（name = 部位键）+ 一个 mesh + 独立 material，
便于查看器/下游按“不同材料组成部分”独立拾取与着色。
"""

from __future__ import annotations

import json
import struct
import numpy as np


def _pad4(b: bytes, fill: bytes = b"\x00") -> bytes:
    return b + fill * ((4 - len(b) % 4) % 4)


def _tri_normals(tri: np.ndarray) -> np.ndarray:
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    ln = np.linalg.norm(n, axis=1)
    ln[ln == 0] = 1.0
    return (n / ln[:, None]).astype(np.float32)


def write_glb_parts(items: list[dict], path: str) -> str:
    """写出多部位 GLB。

    items: 列表，每项为 dict：
        name:     部位键（写入 node.name，必须唯一）
        verts:    [(x,y,z), ...] 或 (N,3) 数组（模型坐标，mm）
        tris:     [(i,j,k), ...] 三角形索引
        color:    (r,g,b) 0~1 材质基色
        material: 材质名（写入 material.name）
        extras:   自定义元数据
    """
    blobs: list[bytes] = []
    views: list[dict] = []
    accessors: list[dict] = []
    meshes: list[dict] = []
    nodes: list[dict] = []
    materials: list[dict] = []
    offset = 0

    def add_view(data: bytes, target: int) -> int:
        nonlocal offset
        pad = _pad4(data)
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": target}
        blobs.append(pad)
        offset += len(pad)
        views.append(view)
        return len(views) - 1

    for it in items:
        V = np.asarray(it["verts"], dtype=np.float32)
        F = np.asarray(it["tris"], dtype=np.uint32)
        if len(V) == 0 or len(F) == 0:
            continue
        tri = V[F]
        pos = tri.reshape(-1, 3).astype(np.float32)
        nor = np.repeat(_tri_normals(tri), 3, axis=0).astype(np.float32)
        ind = np.arange(pos.shape[0], dtype=np.uint32)

        vp = add_view(pos.tobytes(), 34962)
        vn = add_view(nor.tobytes(), 34962)
        vi = add_view(ind.tobytes(), 34963)

        accessors.append({
            "bufferView": vp,
            "componentType": 5126,
            "count": int(pos.shape[0]),
            "type": "VEC3",
            "min": pos.min(axis=0).tolist(),
            "max": pos.max(axis=0).tolist(),
        })
        accessors.append({
            "bufferView": vn,
            "componentType": 5126,
            "count": int(nor.shape[0]),
            "type": "VEC3",
        })
        accessors.append({
            "bufferView": vi,
            "componentType": 5125,
            "count": int(ind.shape[0]),
            "type": "SCALAR",
        })

        c = it.get("color", (0.7, 0.7, 0.72))
        materials.append({
            "name": it.get("material") or it["name"],
            "pbrMetallicRoughness": {
                "baseColorFactor": [float(c[0]), float(c[1]), float(c[2]), 1.0],
                "metallicFactor": 0.0,
                "roughnessFactor": 0.78,
            },
            "doubleSided": False,
        })
        meshes.append({
            "name": it["name"],
            "primitives": [{
                "attributes": {
                    "POSITION": len(accessors) - 3,
                    "NORMAL": len(accessors) - 2,
                },
                "indices": len(accessors) - 1,
                "material": len(materials) - 1,
                "mode": 4,
            }],
        })
        nodes.append({
            "mesh": len(meshes) - 1,
            "name": it["name"],
            "extras": it.get("extras", {}),
        })

    blob = b"".join(blobs)
    gltf = {
        "asset": {"version": "2.0", "generator": "rcif-tools-glb-writer"},
        "scene": 0,
        "scenes": [{"nodes": list(range(len(nodes)))}],
        "nodes": nodes,
        "meshes": meshes,
        "materials": materials,
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }

    json_bytes = _pad4(json.dumps(gltf, separators=(",", ":"), ensure_ascii=False).encode("utf-8"), b" ")
    bin_bytes = _pad4(blob, b"\x00")

    total_len = 12 + 8 + len(json_bytes) + 8 + len(bin_bytes)
    header = struct.pack("<4sII", b"glTF", 2, total_len)
    chunk0 = struct.pack("<I4s", len(json_bytes), b"JSON") + json_bytes
    chunk1 = struct.pack("<I4s", len(bin_bytes), b"BIN\x00") + bin_bytes

    with open(path, "wb") as f:
        f.write(header + chunk0 + chunk1)
    return path
