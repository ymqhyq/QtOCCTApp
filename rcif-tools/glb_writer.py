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


def compute_crease_normals(V: np.ndarray, F: np.ndarray, crease_deg: float = 40.0) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """计算兼顾平滑曲面与锐角硬边的顶点法向量（Crease Angle Smoothing）。

    - 对于曲率平滑段（相邻三角形二面角 <= crease_deg，如圆柱/环面/倒圆角），法向按面面积加权连续过渡；
    - 对于工程硬棱（相邻三角形二面角 > crease_deg，如方盒棱角、90度垂直台阶），保持独立法向硬边。
    """
    v0 = V[F[:, 0]]
    v1 = V[F[:, 1]]
    v2 = V[F[:, 2]]
    fn = np.cross(v1 - v0, v2 - v0)
    fn_len = np.linalg.norm(fn, axis=1, keepdims=True)
    fn_unit = fn / np.where(fn_len == 0, 1.0, fn_len)

    cos_thresh = float(np.cos(np.radians(crease_deg)))
    vert_to_faces: list[list[int]] = [[] for _ in range(len(V))]
    for fi, tri in enumerate(F):
        for vi in tri:
            vert_to_faces[vi].append(fi)

    corner_nor = np.zeros((len(F), 3, 3), dtype=np.float32)
    for fi, tri in enumerate(F):
        f_norm = fn_unit[fi]
        for ci, vi in enumerate(tri):
            shared = vert_to_faces[vi]
            s_fn = fn[shared]
            s_unit = fn_unit[shared]
            dots = np.dot(s_unit, f_norm)
            mask = dots >= cos_thresh
            acc = np.sum(s_fn[mask], axis=0)
            acc_len = np.linalg.norm(acc)
            corner_nor[fi, ci] = acc / (acc_len if acc_len > 0 else 1.0)

    pos = V[F].reshape(-1, 3).astype(np.float32)
    nor = corner_nor.reshape(-1, 3).astype(np.float32)
    ind = np.arange(pos.shape[0], dtype=np.uint32)
    return pos, nor, ind


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

        if "normals" in it and len(it["normals"]) > 0:
            pos = V.astype(np.float32)
            nor = np.asarray(it["normals"], dtype=np.float32)
            ind = F.flatten().astype(np.uint32)
        elif it.get("smoothNormals", True):
            crease_deg = float(it.get("creaseAngle", 40.0))
            pos, nor, ind = compute_crease_normals(V, F, crease_deg=crease_deg)
        else:
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
        alpha = float(c[3]) if len(c) > 3 else 1.0
        metallic = float(it.get("metallic", 0.0))
        roughness = float(it.get("roughness", 0.78))
        alpha_mode = it.get("alphaMode")
        double_sided = bool(it.get("doubleSided", alpha < 1.0))
        mat_entry = {
            "name": it.get("material") or it["name"],
            "pbrMetallicRoughness": {
                "baseColorFactor": [float(c[0]), float(c[1]), float(c[2]), alpha],
                "metallicFactor": metallic,
                "roughnessFactor": roughness,
            },
            "doubleSided": double_sided,
        }
        if alpha_mode:
            mat_entry["alphaMode"] = alpha_mode
        elif alpha < 1.0:
            mat_entry["alphaMode"] = "BLEND"
        materials.append(mat_entry)
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
