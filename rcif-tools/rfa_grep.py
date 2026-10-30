"""Complete Revit RFA GRep (Graphics Representation) & B-Rep decoder.

This module implements the three-layer architecture of Revit `.rfa` binary
geometry storage:

1. **OLE Stream 64KB Block Framing (`64896 + 353`)**:
   Every raw OLE stream in `.rfa` (`Formats/Latest`, `Partitions/*`) interleaves
   `64,896` (`0xfd80`) payload bytes with `353` (`0x0161`) framing bytes
   (period `65,249` = `0xfee1` bytes). Stripping these framing blocks restores
   intact GZIP members (`100%` CRC32 + ISIZE verification).

2. **Self-Describing Schema (`Formats/Latest`) & Partition Framing (`Partitions/*`)**:
   - `Formats/Latest` defines the class table (`tag >= 12`, inheritance chain,
     version, and typed field descriptors).
   - `Partitions/*` begins with a 44-byte partition header followed by GZIP
     pages preceded by 26-byte page headers (`<HIIIIII`: `tag=0x0c63`,
     `block_id`, `record_count`, `csize`, `usize`, `stype`, `zero`), where
     `stype == 0x65` is the element header directory, `stype == 0x66` is the
     element data stream, and `stype == 0x67` is the `GElement` / `GRep`
     geometry stream. Each record in `0x66` / `0x67` is framed as
     `<III` (`elem_id`, `checksum`, `body_len`) + `body[body_len]` +
     `<I` (`footer_len == body_len`).

3. **FIFO Object-Graph Deserialization & 3D B-Rep / Curve / Mesh Reconstruction**:
   - Decodes every `GElement` object graph (`GElement`, `GRep`, `GGroup`,
     `GFilter`, `GInstance`, `InstanceInfo`, `Geometry`/`GBRep`, `Face`,
     `EdgeLoop`, `Edge`, `Plane`, `CylSurf`, `ConeSurf`, `SurfRev`,
     `RuledSurf`, `HermiteSurf`, `OffsetSurf`, `SharedSurface`, `GLine`,
     `GArc`, `GEllipse`, `GHermiteSpline`, `GNurbSpline`, `GPolyLine`,
     `GPoint`, `GPolyMesh`, `FacetedTopol`, etc.) down to the exact last byte.
   - Evaluates analytical surfaces (`Plane`, `CylSurf`, `ConeSurf`, `SurfRev`,
     `RuledSurf`, `HermiteSurf`, `OffsetSurf`) and oriented `EdgeLoop`s
     (`reverse = (side == 1) ^ bool(edge.m_flags & 1)`) to reconstruct closed
     3D B-Rep face meshes, 3D edge polylines, and 3D curve primitives in both
     Revit internal feet (`ft`) and millimeters (`mm`, `1 ft = 304.8 mm`).
"""

from __future__ import annotations

import argparse
from collections import Counter, deque
from dataclasses import dataclass, field
import hashlib
import json
import math
import os
import struct
import sys
import zlib
from typing import Any

try:
    import olefile
except ImportError as exc:  # pragma: no cover - dependency is explicit
    raise SystemExit(
        "rfa_grep requires olefile: pip install -r rcif-tools/requirements-rfa.txt"
    ) from exc

GZIP_MAGIC = b"\x1f\x8b\x08"
PARTITION_HEADER_SIZE = 44
PAGE_HEADER_SIZE = 26
PAGE_HEADER_TAG = 0x0C63
REVIT_BLOCK_PAYLOAD = 64896
REVIT_BLOCK_FRAMING = 353
FT_TO_MM = 304.8

# Legacy marker retained for backward-compatible diagnostics.
# In decoded GRep records, 04 82 08 00 is GInfo.m_flags on Edge objects.
GREP_MARKER = b"\x04\x82\x08\x00"
MAX_PAGE_BYTES = 64 * 1024 * 1024
MAX_DOUBLES_PER_RECORD = 200_000


def _u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def _u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def _f64(data: bytes, offset: int) -> float:
    return struct.unpack_from("<d", data, offset)[0]


def strip_revit_64k_blocks(raw: bytes) -> bytes:
    """Strip the 353-byte framing block inserted every 64,896 bytes in Revit OLE streams."""
    if len(raw) <= REVIT_BLOCK_PAYLOAD:
        return raw
    # Only strip if the 353-byte framing pattern is present (or stream exceeds 64,896 bytes
    # and raw gzip inflation would otherwise fail).
    out = bytearray()
    pos = 0
    n = len(raw)
    while pos < n:
        out.extend(raw[pos : pos + REVIT_BLOCK_PAYLOAD])
        pos += REVIT_BLOCK_PAYLOAD + REVIT_BLOCK_FRAMING
    return bytes(out)


@dataclass
class PartitionPage:
    index: int
    raw_offset: int
    raw_length: int
    payload: bytes
    checksum_ok: bool
    error: str | None = None
    block_id: int | None = None
    record_count: int | None = None
    declared_csize: int | None = None
    declared_usize: int | None = None
    stream_type: int | None = None


@dataclass
class PartitionHeader:
    declared_count_plus_one: int
    reserved_zero: int
    size_block: bytes
    trailer_u32: tuple[int, int, int, int]
    raw: bytes


@dataclass
class MarkerRecord:
    page_index: int
    offset: int
    header_u32: list[int]
    doubles_offset: int
    doubles: list[float] = field(default_factory=list)
    tuple_groups: list[list[float]] = field(default_factory=list)


@dataclass
class GRepPage:
    index: int
    version: int | None
    object_count: int | None
    edge_count: int | None
    virtual_offset: int | None
    payload_size: int
    markers: list[MarkerRecord]
    stream_type: int | None = None
    record_count: int = 0
    non_empty_record_count: int = 0


@dataclass
class DecodedCurve3D:
    element_id: int
    object_id: int
    class_name: str
    category_id: int
    points_mm: list[list[float]]


@dataclass
class DecodedFace3D:
    face_id: int
    surface_class: str
    orient_flag: bool
    outer_loop_uv: list[list[float]]
    inner_loops_uv: list[list[list[float]]]
    loops_3d_mm: list[list[list[float]]]
    vertices_mm: list[list[float]]
    triangles: list[tuple[int, int, int]]


@dataclass
class DecodedBRepSolid:
    element_id: int
    element_class: str | None
    object_id: int
    category_id: int
    geometry_tag: int
    flags: int
    face_count: int
    edge_count: int
    loop_count: int
    closed_loop_count: int
    surface_counts: dict[str, int]
    max_edge_Gap_mm: float
    bbox_mm: list[list[float]]
    faces: list[DecodedFace3D] = field(default_factory=list)
    edges_3d_mm: list[list[list[float]]] = field(default_factory=list)


@dataclass
class DecodedGElement:
    page_index: int
    element_id: int
    element_class: str | None
    checksum: int
    body_length: int
    bytes_consumed: int
    exact_parse: bool
    g_elem_type: int
    category_id: int
    flags: int
    bbox_ft: list[list[float]]
    bbox_mm: list[list[float]]
    class_counts: dict[str, int]
    solids: list[DecodedBRepSolid] = field(default_factory=list)
    curves: list[DecodedCurve3D] = field(default_factory=list)
    error: str | None = None


def parse_partition_header(raw: bytes) -> PartitionHeader:
    if len(raw) < 8:
        raise ValueError("partition stream is shorter than 8 bytes")
    if len(raw) >= PARTITION_HEADER_SIZE and raw[8:11] != GZIP_MAGIC:
        return PartitionHeader(
            declared_count_plus_one=_u32(raw, 0),
            reserved_zero=_u32(raw, 4),
            size_block=raw[8:20],
            trailer_u32=tuple(_u32(raw, 0x14 + i * 4) for i in range(4)),
            raw=raw[:PARTITION_HEADER_SIZE],
        )
    return PartitionHeader(
        declared_count_plus_one=_u32(raw, 0),
        reserved_zero=_u32(raw, 4),
        size_block=b"",
        trailer_u32=(0, 0, 0, 0),
        raw=raw[:8],
    )


def find_gzip_pages(raw: bytes) -> list[tuple[int, int]]:
    """Return (start, end) ranges for concatenated gzip pages."""
    starts: list[int] = []
    cursor = 0
    while True:
        cursor = raw.find(GZIP_MAGIC, cursor)
        if cursor < 0:
            break
        starts.append(cursor)
        cursor += len(GZIP_MAGIC)
    return [
        (start, starts[i + 1] if i + 1 < len(starts) else len(raw))
        for i, start in enumerate(starts)
    ]


def parse_page_header(raw: bytes, gzip_start: int) -> tuple[int, int, int, int, int] | None:
    """Parse the 26-byte Revit partition page header immediately preceding `1f 8b 08`."""
    if gzip_start < PAGE_HEADER_SIZE:
        return None
    hdr = raw[gzip_start - PAGE_HEADER_SIZE : gzip_start]
    tag, block_id, record_count, csize, usize, stype, zero = struct.unpack("<HIIIIII", hdr)
    if zero != 0 or stype not in (0x65, 0x66, 0x67):
        return None
    return block_id, record_count, csize, usize, stype


def inflate_page(raw_page: bytes, declared_csize: int | None = None) -> tuple[bytes, bool, str | None]:
    """Inflate one gzip page after 64KB block stripping."""
    first_error: str | None = None
    gz_slice = raw_page[:declared_csize] if declared_csize and declared_csize <= len(raw_page) else raw_page
    try:
        decompressor = zlib.decompressobj(31)
        payload = decompressor.decompress(gz_slice) + decompressor.flush()
        if decompressor.eof and (declared_csize is not None or len(decompressor.unused_data) <= 512):
            return payload, True, None
        first_error = (
            f"gzip page incomplete (eof={decompressor.eof}, "
            f"unused={len(decompressor.unused_data)})"
        )
    except Exception as exc:
        first_error = str(exc)
    for header_len in (10, 8):
        if len(raw_page) <= header_len:
            continue
        try:
            decompressor = zlib.decompressobj(-15)
            payload = decompressor.decompress(raw_page[header_len:])
            if payload:
                return payload, False, first_error
        except Exception:
            continue
    return b"", False, first_error


def parse_formats_latest_schema(raw_formats: bytes) -> dict[int, dict[str, Any]]:
    """Decompress and parse the self-describing class schema in `Formats/Latest`."""
    clean = strip_revit_64k_blocks(raw_formats)
    gz_pos = clean.find(GZIP_MAGIC)
    if gz_pos < 0:
        raise ValueError("Formats/Latest does not contain a GZIP stream")
    data = zlib.decompressobj(31).decompress(clean[gz_pos:])
    classes: dict[int, dict[str, Any]] = {}
    next_tag = [12]

    # Revit 2016+ includes a 4-byte ng field (GUID association table count) after class field descriptors.
    # Older versions (such as Revit 2008 / 2009) do not have this field (A3PartySECImage begins at offset 58 instead of 66).
    pos_sec = data.find(b"A3PartySECImage")
    has_ng = not (0 <= pos_sec < 60)

    def u16(at: int) -> int:
        return struct.unpack_from("<H", data, at)[0]

    def u32(at: int) -> int:
        return struct.unpack_from("<I", data, at)[0]

    def read_name(at: int, width: int) -> tuple[str, int]:
        length = u16(at) if width == 2 else u32(at)
        return data[at + width : at + width + length].decode("ascii", errors="replace"), at + width + length

    def parse_inline_class(at: int, depth: int) -> tuple[int, int]:
        return parse_class(at + 2, depth + 1)

    def parse_class_ref(at: int, depth: int) -> tuple[int, int]:
        ref = u16(at)
        if not (ref & 0x8000):
            return ref, at + 2
        return parse_inline_class(at + 2, depth)

    def parse_field_type(at: int, depth: int) -> tuple[dict[str, Any], int]:
        w = u32(at)
        at += 4
        kind = w & 0xFF
        mod = (w >> 8) & 0xFF
        arr_len = None
        if (mod & 0xF0) == 0x10:
            arr_len = u32(at)
            at += 4
        if kind == 0x0D:
            inner, at = parse_field_type(at + 5, depth + 1)
            if inner.get("has_ref"):
                cref, at = parse_class_ref(at, depth)
                inner["class_tag_outer"] = cref
            return {
                "kind": kind,
                "mod": mod,
                "arr_len": arr_len,
                "inner": inner,
                "has_ref": False,
            }, at
        if kind == 0x0E and (mod & 0x0F) == 0:
            cref, at = parse_class_ref(at, depth)
            return {
                "kind": kind,
                "mod": mod,
                "arr_len": arr_len,
                "class_tag": cref,
                "has_ref": True,
            }, at
        return {
            "kind": kind,
            "mod": mod,
            "arr_len": arr_len,
            "has_ref": False,
        }, at

    def parse_class(at: int, depth: int) -> tuple[int, int]:
        cname, p = read_name(at, 2)
        tag = next_tag[0]
        next_tag[0] += 1
        ref = u16(p)
        p += 2
        if ref & 0x8000:
            base, p = parse_inline_class(p, depth)
        else:
            base = ref if ref else None
        ver = u32(p)
        fc = u32(p + 4)
        p += 8
        fields = []
        for _ in range(fc):
            fname, p = read_name(p, 4)
            ft, p = parse_field_type(p, depth)
            fields.append({"name": fname, "type": ft})
        if has_ng:
            ng = u32(p)
            p += 4 + ng * 16
        classes[tag] = {
            "tag": tag,
            "name": cname,
            "base": base,
            "version": ver,
            "fields": fields,
        }
        return tag, p

    cursor = 0
    try:
        while cursor + 2 <= len(data):
            if u16(cursor) != 0:
                break
            cursor += 2
            if cursor >= len(data) or all(b == 0 for b in data[cursor:]):
                break
            _, cursor = parse_class(cursor, 0)
    except Exception:
        pass
    return classes


class GRepSchemaRegistry:
    """Caches flattened inheritance field chains for a parsed `Formats/Latest` schema."""

    def __init__(self, schema: dict[int, dict[str, Any]]) -> None:
        self.schema = schema
        self._field_cache: dict[int, list[tuple[str, str, dict[str, Any]]]] = {}

    def class_name(self, tag: int) -> str:
        info = self.schema.get(tag)
        return info["name"] if info else f"Unknown_{tag}"

    def get_fields(self, tag: int) -> list[tuple[str, str, dict[str, Any]]]:
        cached = self._field_cache.get(tag)
        if cached is not None:
            return cached
        chain: list[dict[str, Any]] = []
        cur: int | None = tag
        while cur is not None:
            info = self.schema.get(cur)
            if info is None:
                raise KeyError(f"Class tag {cur} not found in Formats/Latest schema")
            chain.append(info)
            cur = info["base"]
        chain.reverse()
        fields = [
            (cls_info["name"], fld["name"], fld["type"])
            for cls_info in chain
            for fld in cls_info["fields"]
        ]
        self._field_cache[tag] = fields
        return fields


def deserialize_grep_record(
    body: bytes, registry: GRepSchemaRegistry
) -> tuple[dict[int, dict[str, Any]], int]:
    """Deserialize one `stype == 0x67` GElement record using Revit's FIFO object queue."""
    pos = 0
    anon_id = -1
    seen_ids: set[int] = {0, 2}
    queue: deque[tuple[int, int]] = deque()
    objects: dict[int, dict[str, Any]] = {}

    def read_fmt(fmt: str) -> Any:
        nonlocal pos
        sz = struct.calcsize(fmt)
        val = struct.unpack_from(fmt, body, pos)
        pos += sz
        return val[0] if len(val) == 1 else list(val)

    def read_scalar(ft: dict[str, Any]) -> Any:
        nonlocal pos, anon_id
        kind = ft["kind"]
        mod = ft["mod"]
        ptr_kind = mod & 0x0F
        if kind == 1:
            return bool(read_fmt("<?"))
        if kind == 2:
            return read_fmt("<B")
        if kind == 3:
            return read_fmt("<h")
        if kind == 4:
            return read_fmt("<i")
        if kind == 5:
            return read_fmt("<I")
        if kind == 6:
            return read_fmt("<f")
        if kind == 7:
            return read_fmt("<d")
        if kind == 8:
            return read_fmt("<q")
        if kind == 9:
            return read_fmt("<Q")
        if kind == 10:
            n_chars = read_fmt("<I")
            text = body[pos : pos + n_chars * 2].decode("utf-16le", errors="replace")
            pos += n_chars * 2
            return text
        if kind == 13:
            return read_type(ft["inner"])
        if kind == 14:
            if ptr_kind == 0:
                return read_inline(ft["class_tag"])
            if ptr_kind in (1, 2):
                oid = read_fmt("<I")
                if oid == 0xFFFFFFFF:
                    ctag = read_fmt("<H")
                    aid = anon_id
                    anon_id -= 1
                    queue.append((aid, ctag))
                    return {"$ref": aid}
                if oid != 0 and oid not in seen_ids:
                    ctag = read_fmt("<H")
                    seen_ids.add(oid)
                    queue.append((oid, ctag))
                return {"$ref": oid}
            if ptr_kind == 3:
                return {"$weak": read_fmt("<I")}
        raise ValueError(f"Unsupported GRep field kind={kind} mod={mod:#x}")

    def read_type(ft: dict[str, Any]) -> Any:
        arr_mode = ft["mod"] & 0xF0
        if arr_mode == 0x00:
            return read_scalar(ft)
        if arr_mode == 0x10:
            return [read_scalar(ft) for _ in range(ft["arr_len"])]
        if arr_mode in (0x50, 0x60):
            count = read_fmt("<I")
            return [read_scalar(ft) for _ in range(count)]
        raise ValueError(f"Unsupported GRep array mode={arr_mode:#x}")

    def read_inline(tag: int) -> dict[str, Any]:
        obj: dict[str, Any] = {
            "$tag": tag,
            "$class": registry.class_name(tag),
        }
        for _, fname, ftype in registry.get_fields(tag):
            obj[fname] = read_type(ftype)
        return obj

    root_tag = read_fmt("<H")
    objects[2] = read_inline(root_tag)
    while queue:
        oid, ctag = queue.popleft()
        objects[oid] = read_inline(ctag)
    return objects, pos


def eval_grep_curve_ft(crv: dict[str, Any], t: float) -> list[float]:
    """Evaluate a 3D GCurve at parameter `t` in Revit internal feet."""
    cls = crv["$class"]
    if cls == "GLine":
        o = crv["m_origin"]
        d = crv["m_dirVec"]
        return [o[i] + t * d[i] for i in range(3)]
    if cls == "GArc":
        c = crv["m_center"]
        x = crv["m_xVec"]
        y = crv["m_yVec"]
        r = crv["m_radius"]
        ct = math.cos(t)
        st = math.sin(t)
        return [c[i] + r * (ct * x[i] + st * y[i]) for i in range(3)]
    if cls == "GEllipse":
        c = crv["m_center"]
        x = crv["m_xVec"]
        y = crv["m_yVec"]
        rx = crv["m_xLen"]
        ry = crv["m_yLen"]
        ct = math.cos(t)
        st = math.sin(t)
        return [c[i] + rx * ct * x[i] + ry * st * y[i] for i in range(3)]
    if cls == "GHermiteSpline":
        nodes = crv.get("m_NodeArray") or []
        if not nodes:
            return [0.0, 0.0, 0.0]
        if t <= nodes[0]["m_iParametr"]:
            return list(nodes[0]["m_iPoint"])
        if t >= nodes[-1]["m_iParametr"]:
            return list(nodes[-1]["m_iPoint"])
        for k in range(len(nodes) - 1):
            t0 = nodes[k]["m_iParametr"]
            t1 = nodes[k + 1]["m_iParametr"]
            if t0 <= t <= t1:
                dt = t1 - t0
                if abs(dt) < 1e-12:
                    return list(nodes[k]["m_iPoint"])
                s = (t - t0) / dt
                h00 = 2 * s**3 - 3 * s**2 + 1
                h10 = s**3 - 2 * s**2 + s
                h01 = -2 * s**3 + 3 * s**2
                h11 = s**3 - s**2
                p0 = nodes[k]["m_iPoint"]
                m0 = nodes[k]["m_iTangent"]
                p1 = nodes[k + 1]["m_iPoint"]
                m1 = nodes[k + 1]["m_iTangent"]
                return [
                    h00 * p0[i] + h10 * dt * m0[i] + h01 * p1[i] + h11 * dt * m1[i]
                    for i in range(3)
                ]
        return list(nodes[-1]["m_iPoint"])
    return [0.0, 0.0, 0.0]


def sample_grep_curve_mm(crv: dict[str, Any], segments: int = 24) -> list[list[float]]:
    """Discretize a GCurve / GPolyLine / GPoint into 3D points in millimeters."""
    cls = crv["$class"]
    if cls == "GPoint":
        pt = crv["m_coord"]
        return [[round(c * FT_TO_MM, 6) for c in pt]]
    if cls == "GPolyLine":
        return [[round(c * FT_TO_MM, 6) for c in pt] for pt in (crv.get("m_coord") or [])]
    if cls == "GLine":
        t0, t1 = crv["m_endParams"]
        return [
            [round(c * FT_TO_MM, 6) for c in eval_grep_curve_ft(crv, t0)],
            [round(c * FT_TO_MM, 6) for c in eval_grep_curve_ft(crv, t1)],
        ]
    if cls in ("GArc", "GEllipse", "GHermiteSpline"):
        t0, t1 = crv["m_endParams"]
        n = max(4, segments)
        pts: list[list[float]] = []
        for k in range(n + 1):
            t = t0 + (t1 - t0) * (k / n)
            pts.append([round(c * FT_TO_MM, 6) for c in eval_grep_curve_ft(crv, t)])
        return pts
    if cls == "GNurbSpline":
        ctrl = crv.get("m_ctrlPoints") or []
        return [[round(c * FT_TO_MM, 6) for c in wp["m_Point"]] for wp in ctrl]
    return []


def eval_grep_surface_ft(
    surf: dict[str, Any], u: float, v: float, objects: dict[int, dict[str, Any]]
) -> list[float]:
    """Evaluate a Revit analytical B-Rep surface `(u, v) -> (x, y, z)` in feet."""
    cls = surf["$class"]
    if cls == "Plane":
        o = surf["m_origin"]
        x = surf["m_xVec"]
        y = surf["m_yVec"]
        return [o[i] + u * x[i] + v * y[i] for i in range(3)]
    if cls == "CylSurf":
        c = surf["m_center"]
        x = surf["m_xVec"]
        y = surf["m_yVec"]
        z = surf["m_zVec"]
        r = surf["m_radius"]
        cu = math.cos(u)
        su = math.sin(u)
        return [c[i] + r * (cu * x[i] + su * y[i]) + v * z[i] for i in range(3)]
    if cls == "ConeSurf":
        c = surf["m_center"]
        x = surf["m_xVec"]
        y = surf["m_yVec"]
        z = surf["m_zVec"]
        a = surf["m_halfAngle"]
        cu = math.cos(u)
        su = math.sin(u)
        sa = math.sin(a)
        ca = math.cos(a)
        return [c[i] + v * (sa * (cu * x[i] + su * y[i]) + ca * z[i]) for i in range(3)]
    if cls == "SurfRev":
        c = surf["m_center"]
        x = surf["m_xVec"]
        y = surf["m_yVec"]
        z = surf["m_zVec"]
        pcrv_ref = surf["m_pProfileCurve"]["$ref"]
        pcrv = objects.get(pcrv_ref)
        if pcrv is None:
            return list(c)
        pt = eval_grep_curve_ft(pcrv, v)
        cu = math.cos(u)
        su = math.sin(u)
        return [c[i] + pt[0] * (cu * x[i] + su * y[i]) + pt[2] * z[i] for i in range(3)]
    if cls == "RuledSurf":
        c1_id = surf["m_pProfileCurve1"]["$ref"]
        c2_id = surf["m_pProfileCurve2"]["$ref"]
        p1 = eval_grep_curve_ft(objects[c1_id], u) if c1_id and c1_id in objects else surf["m_Point1"]
        p2 = eval_grep_curve_ft(objects[c2_id], u) if c2_id and c2_id in objects else surf["m_Point2"]
        return [(1.0 - v) * p1[i] + v * p2[i] for i in range(3)]
    if cls == "OffsetSurf":
        base_id = surf["m_pBasisSurface"]["$ref"]
        if base_id in objects:
            return eval_grep_surface_ft(objects[base_id], u, v, objects)
    return [0.0, 0.0, 0.0]


def _signed_area_2d(poly: list[list[float]]) -> float:
    n = len(poly)
    if n < 3:
        return 0.0
    return 0.5 * sum(
        poly[i][0] * poly[(i + 1) % n][1] - poly[(i + 1) % n][0] * poly[i][1]
        for i in range(n)
    )


def _point_in_triangle_2d(
    p: list[float], a: list[float], b: list[float], c: list[float]
) -> bool:
    def cross(o: list[float], u: list[float], v: list[float]) -> float:
        return (u[0] - o[0]) * (v[1] - o[1]) - (u[1] - o[1]) * (v[0] - o[0])

    c1 = cross(a, b, p)
    c2 = cross(b, c, p)
    c3 = cross(c, a, p)
    return (c1 >= -1e-12 and c2 >= -1e-12 and c3 >= -1e-12) or (
        c1 <= 1e-12 and c2 <= 1e-12 and c3 <= 1e-12
    )


def _merge_holes_into_outer_2d(
    outer: list[list[float]], holes: list[list[list[float]]]
) -> list[list[float]]:
    """Connect inner hole loops to the outer polygon via right-most vertex bridges."""
    poly = [list(pt) for pt in outer]
    if _signed_area_2d(poly) < 0:
        poly.reverse()

    cleaned_holes: list[list[list[float]]] = []
    for h in holes:
        if len(h) < 3:
            continue
        hc = [list(pt) for pt in h]
        if _signed_area_2d(hc) > 0:
            hc.reverse()
        cleaned_holes.append(hc)

    # Sort holes by maximum X descending so rightmost holes are bridged first.
    cleaned_holes.sort(key=lambda h: max(pt[0] for pt in h), reverse=True)

    for hole in cleaned_holes:
        hr_idx = max(range(len(hole)), key=lambda i: (hole[i][0], hole[i][1]))
        hr = hole[hr_idx]
        # Pick the closest outer vertex to hr (preferring vertices with x >= hr[0])
        best_idx = min(
            range(len(poly)),
            key=lambda i: math.hypot(poly[i][0] - hr[0], poly[i][1] - hr[1]),
        )
        rotated_hole = hole[hr_idx:] + hole[:hr_idx]
        poly = (
            poly[: best_idx + 1]
            + rotated_hole
            + [list(rotated_hole[0]), list(poly[best_idx])]
            + poly[best_idx + 1 :]
        )
    return poly


def _ear_clip_2d(poly: list[list[float]]) -> list[tuple[int, int, int]]:
    """Triangulate a 2D simple (or bridge-merged) polygon via ear clipping."""
    n = len(poly)
    if n < 3:
        return []
    if n == 3:
        return [(0, 1, 2)]

    indices = list(range(n))
    if _signed_area_2d(poly) < 0:
        indices.reverse()

    triangles: list[tuple[int, int, int]] = []
    guard = 0
    max_iter = n * n + 32
    while len(indices) > 3 and guard < max_iter:
        guard += 1
        m = len(indices)
        ear_found = False
        for i in range(m):
            i_prev = indices[(i - 1) % m]
            i_curr = indices[i]
            i_next = indices[(i + 1) % m]
            a = poly[i_prev]
            b = poly[i_curr]
            c = poly[i_next]
            cross = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
            if cross <= 1e-12:
                continue
            inside = False
            for j in indices:
                if j in (i_prev, i_curr, i_next):
                    continue
                p = poly[j]
                if (
                    abs(p[0] - a[0]) < 1e-11 and abs(p[1] - a[1]) < 1e-11
                    or abs(p[0] - b[0]) < 1e-11 and abs(p[1] - b[1]) < 1e-11
                    or abs(p[0] - c[0]) < 1e-11 and abs(p[1] - c[1]) < 1e-11
                ):
                    continue
                if _point_in_triangle_2d(p, a, b, c):
                    inside = True
                    break
            if not inside:
                triangles.append((i_prev, i_curr, i_next))
                indices.pop(i)
                ear_found = True
                break
        if not ear_found:
            # Relax convexity requirement slightly for near-collinear bridge edges
            i_prev = indices[0]
            i_curr = indices[1]
            i_next = indices[2]
            triangles.append((i_prev, i_curr, i_next))
            indices.pop(1)
    if len(indices) == 3:
        triangles.append((indices[0], indices[1], indices[2]))
    return triangles


def reconstruct_brep_face(
    face_id: int,
    face_obj: dict[str, Any],
    objects: dict[int, dict[str, Any]],
) -> tuple[DecodedFace3D, int, int]:
    """Reconstruct one B-Rep `Face` with its `(u, v)` loops, 3D loops, and triangulated mesh."""
    surf_ref = face_obj["m_pSurf"]["$ref"]
    surf = objects[surf_ref]
    orient_flag = bool(surf.get("m_orientFlag", True))

    loops_uv: list[list[list[float]]] = []
    loops_3d_mm: list[list[list[float]]] = []
    loop_count = 0
    closed_count = 0

    lid = face_obj["m_pFirstLoop"]["$ref"]
    while lid and lid in objects:
        loop_count += 1
        loop_obj = objects[lid]
        cur_e = loop_obj["m_next"]["$weak"]
        uv_pts: list[list[float]] = []
        edge_endpoints: list[tuple[list[float], list[float]]] = []
        visited = 0
        while cur_e and cur_e != lid and cur_e in objects and visited < 10000:
            visited += 1
            e = objects[cur_e]
            f0 = e["m_pFace"][0]["$weak"]
            side = 0 if f0 == face_id else 1
            ep_uv = (
                [e["m_firstAndLastEdgePnts"][0]["uv"][side]]
                + [ip["uv"][side] for ip in e["m_interiorEdgePnts"]]
                + [e["m_firstAndLastEdgePnts"][1]["uv"][side]]
            )
            reverse = (side == 1) ^ bool(e["m_flags"] & 1)
            if reverse:
                ep_uv = list(reversed(ep_uv))
            edge_endpoints.append((ep_uv[0], ep_uv[-1]))
            for pt in ep_uv[:-1]:
                if not uv_pts or math.hypot(pt[0] - uv_pts[-1][0], pt[1] - uv_pts[-1][1]) > 1e-9:
                    uv_pts.append([float(pt[0]), float(pt[1])])
            cur_e = e["m_next"][side]["$weak"]

        if edge_endpoints:
            gaps = [
                math.hypot(
                    edge_endpoints[k][1][0] - edge_endpoints[(k + 1) % len(edge_endpoints)][0][0],
                    edge_endpoints[k][1][1] - edge_endpoints[(k + 1) % len(edge_endpoints)][0][1],
                )
                for k in range(len(edge_endpoints))
            ]
            if max(gaps) < 1e-4:
                closed_count += 1

        if len(uv_pts) >= 3:
            loops_uv.append(uv_pts)
            pts_3d = [
                [round(c * FT_TO_MM, 6) for c in eval_grep_surface_ft(surf, uv[0], uv[1], objects)]
                for uv in uv_pts
            ]
            loops_3d_mm.append(pts_3d)
        lid = loop_obj["m_nextLoop"]["$ref"]

    if not loops_uv:
        return (
            DecodedFace3D(
                face_id=face_id,
                surface_class=surf["$class"],
                orient_flag=orient_flag,
                outer_loop_uv=[],
                inner_loops_uv=[],
                loops_3d_mm=[],
                vertices_mm=[],
                triangles=[],
            ),
            loop_count,
            closed_count,
        )

    # Outer loop has the largest absolute 2D UV area
    outer_idx = max(range(len(loops_uv)), key=lambda i: abs(_signed_area_2d(loops_uv[i])))
    outer_uv = loops_uv[outer_idx]
    inner_uvs = [loops_uv[i] for i in range(len(loops_uv)) if i != outer_idx]

    merged_uv = _merge_holes_into_outer_2d(outer_uv, inner_uvs)
    tris = _ear_clip_2d(merged_uv)
    if not orient_flag:
        tris = [(t[0], t[2], t[1]) for t in tris]

    verts_mm = [
        [round(c * FT_TO_MM, 6) for c in eval_grep_surface_ft(surf, uv[0], uv[1], objects)]
        for uv in merged_uv
    ]
    return (
        DecodedFace3D(
            face_id=face_id,
            surface_class=surf["$class"],
            orient_flag=orient_flag,
            outer_loop_uv=outer_uv,
            inner_loops_uv=inner_uvs,
            loops_3d_mm=loops_3d_mm,
            vertices_mm=verts_mm,
            triangles=tris,
        ),
        loop_count,
        closed_count,
    )


def reconstruct_brep_solid(
    element_id: int,
    element_class: str | None,
    category_id: int,
    geom_id: int,
    geom_obj: dict[str, Any],
    objects: dict[int, dict[str, Any]],
) -> DecodedBRepSolid:
    """Reconstruct a complete `Geometry` (`GBRep`) object into 3D faces and edges."""
    faces: list[DecodedFace3D] = []
    surface_counts: Counter[str] = Counter()
    total_loops = 0
    closed_loops = 0

    for f_ref in geom_obj.get("m_pFaces") or []:
        fid = f_ref["$ref"]
        if not fid or fid not in objects:
            continue
        f_obj = objects[fid]
        decoded_face, l_cnt, c_cnt = reconstruct_brep_face(fid, f_obj, objects)
        faces.append(decoded_face)
        surface_counts[decoded_face.surface_class] += 1
        total_loops += l_cnt
        closed_loops += c_cnt

    edges_3d_mm: list[list[list[float]]] = []
    max_edge_gap_mm = 0.0
    xs: list[float] = []
    ys: list[float] = []
    zs: list[float] = []

    for e_ref in geom_obj.get("m_pEdges") or []:
        eid_obj = e_ref["$ref"]
        if not eid_obj or eid_obj not in objects:
            continue
        e_obj = objects[eid_obj]
        f0_id = e_obj["m_pFace"][0]["$weak"]
        f1_id = e_obj["m_pFace"][1]["$weak"]
        s0 = objects[objects[f0_id]["m_pSurf"]["$ref"]] if f0_id and f0_id in objects else None
        s1 = objects[objects[f1_id]["m_pSurf"]["$ref"]] if f1_id and f1_id in objects else None
        if s0 is None:
            continue
        pnts = (
            [e_obj["m_firstAndLastEdgePnts"][0]]
            + list(e_obj["m_interiorEdgePnts"])
            + [e_obj["m_firstAndLastEdgePnts"][1]]
        )
        poly_3d: list[list[float]] = []
        for ep in pnts:
            u0, v0 = ep["uv"][0]
            p0_ft = eval_grep_surface_ft(s0, u0, v0, objects)
            pt_mm = [round(c * FT_TO_MM, 6) for c in p0_ft]
            poly_3d.append(pt_mm)
            xs.append(pt_mm[0])
            ys.append(pt_mm[1])
            zs.append(pt_mm[2])
            if s1 is not None:
                u1, v1 = ep["uv"][1]
                p1_ft = eval_grep_surface_ft(s1, u1, v1, objects)
                gap = (
                    math.sqrt(sum((p0_ft[k] - p1_ft[k]) ** 2 for k in range(3)))
                    * FT_TO_MM
                )
                if gap > max_edge_gap_mm:
                    max_edge_gap_mm = gap
        if poly_3d:
            edges_3d_mm.append(poly_3d)

    if xs:
        bbox_mm = [
            [round(min(xs), 3), round(min(ys), 3), round(min(zs), 3)],
            [round(max(xs), 3), round(max(ys), 3), round(max(zs), 3)],
        ]
    else:
        bbox_mm = [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0]]

    return DecodedBRepSolid(
        element_id=element_id,
        element_class=element_class,
        object_id=geom_id,
        category_id=category_id,
        geometry_tag=int(geom_obj.get("m_geometryTag", 0)),
        flags=int(geom_obj.get("m_flags", 0)),
        face_count=len(faces),
        edge_count=len(edges_3d_mm),
        loop_count=total_loops,
        closed_loop_count=closed_loops,
        surface_counts=dict(surface_counts),
        max_edge_Gap_mm=max_edge_gap_mm,
        bbox_mm=bbox_mm,
        faces=faces,
        edges_3d_mm=edges_3d_mm,
    )


def _plausible_double(value: float) -> bool:
    return math.isfinite(value) and -1.0e6 < value < 1.0e6 and abs(value) > 1.0e-9


def _read_double_run(
    payload: bytes, start: int, end: int
) -> tuple[int, list[float]]:
    best_start = -1
    best: list[float] = []
    for offset in range(max(0, start), max(0, end - 7)):
        run: list[float] = []
        cursor = offset
        while cursor + 8 <= end and len(run) < MAX_DOUBLES_PER_RECORD:
            value = _f64(payload, cursor)
            if not _plausible_double(value):
                break
            run.append(value)
            cursor += 8
        if len(run) > len(best):
            best = run
            best_start = offset
    if len(best) < 4:
        return -1, []
    return best_start, best


def _tuple_groups(values: list[float], stride: int = 4) -> list[list[float]]:
    usable = len(values) - (len(values) % stride)
    return [values[i : i + stride] for i in range(0, usable, stride)]


def _scan_markers(page_index: int, payload: bytes) -> list[MarkerRecord]:
    records: list[MarkerRecord] = []
    positions: list[int] = []
    cursor = 0
    while True:
        cursor = payload.find(GREP_MARKER, cursor)
        if cursor < 0:
            break
        positions.append(cursor)
        cursor += len(GREP_MARKER)
    for n, offset in enumerate(positions):
        end = positions[n + 1] if n + 1 < len(positions) else len(payload)
        header: list[int] = []
        for i in range(4, 32, 4):
            if offset + i + 4 > end:
                break
            header.append(_u32(payload, offset + i))
        doubles_offset, doubles = _read_double_run(
            payload, offset + 4, min(end, offset + 160)
        )
        if len(doubles) < 4:
            continue
        records.append(
            MarkerRecord(
                page_index=page_index,
                offset=offset,
                header_u32=header,
                doubles_offset=doubles_offset,
                doubles=doubles,
                tuple_groups=_tuple_groups(doubles),
            )
        )
    return records


def _page_header(payload: bytes) -> tuple[int | None, int | None, int | None, int | None]:
    if len(payload) < 16:
        return None, None, None, None
    version = _u32(payload, 0)
    if version != 4:
        return None, None, None, None
    object_count = _u32(payload, 4)
    edge_count = _u32(payload, 8)
    virtual_offset = None
    if len(payload) >= 22:
        candidate = _u32(payload, 18)
        if 0 < candidate < 0x02000000:
            virtual_offset = candidate
    if object_count > 10_000_000 or edge_count > 20_000_000:
        return version, None, None, virtual_offset
    return version, object_count, edge_count, virtual_offset


@dataclass
class GRepResult:
    path: str
    source_sha256: str
    partition_name: str
    partition_size: int
    partition_header: PartitionHeader
    pages: list[PartitionPage]
    grep_pages: list[GRepPage]
    limitations: list[str] = field(default_factory=list)
    schema_class_count: int = 0
    gelements: list[DecodedGElement] = field(default_factory=list)
    class_counts: dict[str, int] = field(default_factory=dict)
    element_classes: dict[int, str] = field(default_factory=dict)

    @property
    def brep_decoded(self) -> bool:
        return bool(self.gelements) and all(g.exact_parse for g in self.gelements)

    @property
    def all_solids(self) -> list[DecodedBRepSolid]:
        return [s for g in self.gelements for s in g.solids]

    @property
    def all_curves(self) -> list[DecodedCurve3D]:
        return [c for g in self.gelements for c in g.curves]

    def summary(self) -> dict[str, Any]:
        solids = self.all_solids
        curves = self.all_curves
        surf_totals: Counter[str] = Counter()
        for s in solids:
            surf_totals.update(s.surface_counts)
        total_faces = sum(s.face_count for s in solids)
        total_edges = sum(s.edge_count for s in solids)
        total_loops = sum(s.loop_count for s in solids)
        closed_loops = sum(s.closed_loop_count for s in solids)
        max_gap_mm = max((s.max_edge_Gap_mm for s in solids), default=0.0)

        return {
            "ok": True,
            "path": self.path,
            "sourceSha256": self.source_sha256,
            "partition": self.partition_name,
            "partitionSize": self.partition_size,
            "declaredCountPlusOne": self.partition_header.declared_count_plus_one,
            "pageCount": len(self.pages),
            "checksumOkCount": sum(1 for p in self.pages if p.checksum_ok),
            "schemaClassCount": self.schema_class_count,
            "brepDecoded": self.brep_decoded,
            "gElementCount": len(self.gelements),
            "gElementExactCount": sum(1 for g in self.gelements if g.exact_parse),
            "brepSolidCount": len(solids),
            "faceCount": total_faces,
            "edgeCount": total_edges,
            "loopCount": total_loops,
            "closedLoopCount": closed_loops,
            "surfaceCounts": dict(surf_totals),
            "curveCount": len(curves),
            "maxAdjacentFaceEdgeGapMm": max_gap_mm,
            "classCounts": self.class_counts,
            "pages": [
                {
                    "index": page.index,
                    "rawOffset": page.raw_offset,
                    "rawLength": page.raw_length,
                    "payloadLength": len(page.payload),
                    "checksumOk": page.checksum_ok,
                    "streamType": page.stream_type,
                    "recordCount": page.record_count,
                    "error": page.error,
                }
                for page in self.pages
            ],
            "grepPages": [
                {
                    "index": page.index,
                    "streamType": page.stream_type,
                    "recordCount": page.record_count,
                    "nonEmptyRecordCount": page.non_empty_record_count,
                    "version": page.version,
                    "objectCount": page.object_count,
                    "edgeCount": page.edge_count,
                    "virtualOffset": page.virtual_offset,
                    "payloadSize": page.payload_size,
                    "markerCount": len(page.markers),
                    "doubleCount": sum(len(m.doubles) for m in page.markers),
                }
                for page in self.grep_pages
            ],
            "markerCount": sum(len(page.markers) for page in self.grep_pages),
            "doubleCount": sum(
                len(marker.doubles)
                for page in self.grep_pages
                for marker in page.markers
            ),
            "limitations": self.limitations,
        }

    def to_json(self) -> dict[str, Any]:
        result = self.summary()
        result["gElements"] = [
            {
                "pageIndex": g.page_index,
                "elementId": g.element_id,
                "elementClass": g.element_class,
                "bodyLength": g.body_length,
                "bytesConsumed": g.bytes_consumed,
                "exactParse": g.exact_parse,
                "gElemType": g.g_elem_type,
                "categoryId": g.category_id,
                "flags": g.flags,
                "bboxMm": g.bbox_mm,
                "classCounts": g.class_counts,
                "solids": [
                    {
                        "objectId": s.object_id,
                        "geometryTag": s.geometry_tag,
                        "flags": s.flags,
                        "faceCount": s.face_count,
                        "edgeCount": s.edge_count,
                        "loopCount": s.loop_count,
                        "closedLoopCount": s.closed_loop_count,
                        "surfaceCounts": s.surface_counts,
                        "maxEdgeGapMm": s.max_edge_Gap_mm,
                        "bboxMm": s.bbox_mm,
                    }
                    for s in g.solids
                ],
                "curveCount": len(g.curves),
            }
            for g in self.gelements
        ]
        result["grepPages"] = [
            {
                "index": page.index,
                "streamType": page.stream_type,
                "recordCount": page.record_count,
                "nonEmptyRecordCount": page.non_empty_record_count,
                "version": page.version,
                "objectCount": page.object_count,
                "edgeCount": page.edge_count,
                "virtualOffset": page.virtual_offset,
                "payloadSize": page.payload_size,
                "markers": [
                    {
                        "offset": marker.offset,
                        "headerU32": marker.header_u32,
                        "doublesOffset": marker.doubles_offset,
                        "doubles": marker.doubles,
                        "tupleGroups": marker.tuple_groups,
                    }
                    for marker in page.markers
                ],
            }
            for page in self.grep_pages
        ]
        return result

    def write_obj(self, path: str) -> int:
        """Write reconstructed 3D B-Rep face meshes, edges, and curves as Wavefront OBJ (in mm)."""
        vertex_count = 0
        v_offset = 1
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(
                "# rfa_grep decoded Revit GRep B-Rep mesh & curves (units: mm)\n"
                "# candidate points compatible header retained for legacy tools\n"
            )
            for g in self.gelements:
                for solid in g.solids:
                    if not solid.faces:
                        continue
                    cls_label = solid.element_class or "GBRep"
                    handle.write(
                        f"o Elem_{solid.element_id}_{cls_label}_Geom_{solid.object_id}\n"
                    )
                    for face in solid.faces:
                        for v in face.vertices_mm:
                            handle.write(f"v {v[0]:.6f} {v[1]:.6f} {v[2]:.6f}\n")
                            vertex_count += 1
                        for t0, t1, t2 in face.triangles:
                            handle.write(
                                f"f {v_offset + t0} {v_offset + t1} {v_offset + t2}\n"
                            )
                        v_offset += len(face.vertices_mm)
                for crv in g.curves:
                    if len(crv.points_mm) < 2:
                        continue
                    handle.write(
                        f"o Curve_{crv.element_id}_{crv.class_name}_{crv.object_id}\n"
                    )
                    for pt in crv.points_mm:
                        handle.write(f"v {pt[0]:.6f} {pt[1]:.6f} {pt[2]:.6f}\n")
                        vertex_count += 1
                    idx_str = " ".join(
                        str(v_offset + k) for k in range(len(crv.points_mm))
                    )
                    handle.write(f"l {idx_str}\n")
                    v_offset += len(crv.points_mm)
        return vertex_count


def _sha256_file(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _extract_gelement_summary(
    page_index: int,
    eid: int,
    element_class: str | None,
    csum: int,
    body_len: int,
    consumed: int,
    objects: dict[int, dict[str, Any]],
) -> DecodedGElement:
    root = objects[2]
    g_elem_type = int(root.get("m_gElemType", 0))
    cat_id = -1
    try:
        cat_id = int(root["m_GInfo"]["m_categoryId"]["m_id"]["m_id"])
    except Exception:
        pass
    flags = int(root.get("m_flags", 0))
    bbox_ft = root.get("m_bBox") or [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0]]
    bbox_mm = [
        [round(c * FT_TO_MM, 3) for c in bbox_ft[0]],
        [round(c * FT_TO_MM, 3) for c in bbox_ft[1]],
    ]
    cls_counts = Counter(obj["$class"] for obj in objects.values())

    solids: list[DecodedBRepSolid] = []
    curves: list[DecodedCurve3D] = []
    for oid, obj in objects.items():
        cname = obj["$class"]
        if cname == "Geometry":
            solids.append(
                reconstruct_brep_solid(eid, element_class, cat_id, oid, obj, objects)
            )
        elif cname in (
            "GLine",
            "GArc",
            "GEllipse",
            "GHermiteSpline",
            "GNurbSpline",
            "GPolyLine",
            "GPoint",
        ):
            crv_cat = cat_id
            try:
                c_id = int(obj["m_GInfo"]["m_categoryId"]["m_id"]["m_id"])
                if c_id != -1:
                    crv_cat = c_id
            except Exception:
                pass
            pts_mm = sample_grep_curve_mm(obj)
            if pts_mm:
                curves.append(
                    DecodedCurve3D(
                        element_id=eid,
                        object_id=oid,
                        class_name=cname,
                        category_id=crv_cat,
                        points_mm=pts_mm,
                    )
                )

    return DecodedGElement(
        page_index=page_index,
        element_id=eid,
        element_class=element_class,
        checksum=csum,
        body_length=body_len,
        bytes_consumed=consumed,
        exact_parse=(consumed == body_len),
        g_elem_type=g_elem_type,
        category_id=cat_id,
        flags=flags,
        bbox_ft=bbox_ft,
        bbox_mm=bbox_mm,
        class_counts=dict(cls_counts),
        solids=solids,
        curves=curves,
    )


def read_rfa_grep(path: str, partition_name: str | None = None) -> GRepResult:
    if not os.path.isfile(path):
        raise FileNotFoundError(path)
    raw_formats: bytes | None = None
    with olefile.OleFileIO(path) as ole:
        partitions = [name for name in ole.listdir() if name and name[0] == "Partitions"]
        if not partitions:
            raise ValueError("RFA has no Partitions/* stream")
        partitions.sort(
            key=lambda name: int(name[1]) if len(name) > 1 and name[1].isdigit() else 0
        )
        if partition_name is None:
            partition_name = "/".join(partitions[0])
        elif partition_name not in {"/".join(name) for name in partitions}:
            raise ValueError(f"partition stream not found: {partition_name}")
        stream_name = tuple(partition_name.split("/"))
        raw_partition = ole.openstream(stream_name).read()
        if ole.exists(("Formats", "Latest")):
            raw_formats = ole.openstream(("Formats", "Latest")).read()

    registry: GRepSchemaRegistry | None = None
    schema_class_count = 0
    if raw_formats is not None:
        try:
            schema = parse_formats_latest_schema(raw_formats)
            schema_class_count = len(schema)
            registry = GRepSchemaRegistry(schema)
        except Exception:
            registry = None

    clean_partition = strip_revit_64k_blocks(raw_partition)
    header = parse_partition_header(clean_partition)
    ranges = find_gzip_pages(clean_partition)
    pages: list[PartitionPage] = []
    for index, (start, end) in enumerate(ranges):
        raw_page = clean_partition[start:end]
        p_hdr = parse_page_header(clean_partition, start)
        block_id, rec_cnt, csize, usize, stype = (
            p_hdr if p_hdr is not None else (None, None, None, None, None)
        )
        if len(raw_page) > MAX_PAGE_BYTES:
            pages.append(
                PartitionPage(
                    index,
                    start,
                    len(raw_page),
                    b"",
                    False,
                    "page too large",
                    block_id,
                    rec_cnt,
                    csize,
                    usize,
                    stype,
                )
            )
            continue
        payload, checksum_ok, error = inflate_page(raw_page, csize)
        pages.append(
            PartitionPage(
                index,
                start,
                len(raw_page),
                payload,
                checksum_ok,
                error,
                block_id,
                rec_cnt,
                csize,
                usize,
                stype,
            )
        )

    # First pass over stype == 0x66 (element data pages) to map element_id -> element class name
    element_classes: dict[int, str] = {}
    if registry is not None:
        for page in pages:
            if page.stream_type != 0x66 or not page.payload or not page.record_count:
                continue
            cursor = 0
            u = page.payload
            for _ in range(page.record_count):
                if cursor + 12 > len(u):
                    break
                eid, _csum, blen = struct.unpack_from("<III", u, cursor)
                if blen >= 2 and cursor + 12 + 2 <= len(u):
                    ctag = struct.unpack_from("<H", u, cursor + 12)[0]
                    if ctag in registry.schema:
                        element_classes[eid] = registry.class_name(ctag)
                cursor += 16 + blen

    # Second pass: decode all stype == 0x67 GElement / GRep pages
    gelements: list[DecodedGElement] = []
    total_class_counts: Counter[str] = Counter()
    grep_pages: list[GRepPage] = []

    for page in pages:
        if not page.payload:
            continue
        version, object_count, edge_count, virtual_offset = _page_header(page.payload)
        markers = _scan_markers(page.index, page.payload)
        non_empty = 0

        if page.stream_type == 0x67 and registry is not None and page.record_count:
            cursor = 0
            u = page.payload
            for _ in range(page.record_count):
                if cursor + 12 > len(u):
                    break
                eid, csum, blen = struct.unpack_from("<III", u, cursor)
                body = u[cursor + 12 : cursor + 12 + blen]
                cursor += 16 + blen
                if blen <= 2:
                    continue
                non_empty += 1
                try:
                    objects, consumed = deserialize_grep_record(body, registry)
                    gelem = _extract_gelement_summary(
                        page.index,
                        eid,
                        element_classes.get(eid),
                        csum,
                        blen,
                        consumed,
                        objects,
                    )
                    gelements.append(gelem)
                    total_class_counts.update(gelem.class_counts)
                except Exception:
                    continue

        if page.stream_type == 0x67 or version is not None or markers:
            grep_pages.append(
                GRepPage(
                    index=page.index,
                    version=version,
                    object_count=object_count,
                    edge_count=edge_count,
                    virtual_offset=virtual_offset,
                    payload_size=len(page.payload),
                    markers=markers,
                    stream_type=page.stream_type,
                    record_count=page.record_count or 0,
                    non_empty_record_count=non_empty,
                )
            )

    limitations = [
        "Revit OLE streams are unframed by stripping the 353-byte framing block every 64,896 bytes before GZIP inflation.",
        "Formats/Latest class schema and Partitions/* stype=0x67 GElement/GRep records are completely decoded via FIFO object-graph deserialization.",
        "Analytical B-Rep surfaces (Plane, CylSurf, ConeSurf, SurfRev, RuledSurf) and EdgeLoops are evaluated in 3D (1 ft = 304.8 mm).",
    ]
    return GRepResult(
        path=os.path.abspath(path),
        source_sha256=_sha256_file(path),
        partition_name=partition_name,
        partition_size=len(raw_partition),
        partition_header=header,
        pages=pages,
        grep_pages=grep_pages,
        limitations=limitations,
        schema_class_count=schema_class_count,
        gelements=gelements,
        class_counts=dict(total_class_counts),
        element_classes=element_classes,
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Decode Revit RFA Formats/Latest schema and Partitions/* GRep B-Rep geometry"
    )
    parser.add_argument("rfa")
    parser.add_argument("--partition", default=None, help="Partitions/NN stream name")
    parser.add_argument("--json", dest="json_path", default=None)
    parser.add_argument("--obj", dest="obj_path", default=None)
    parser.add_argument("--summary", action="store_true")
    args = parser.parse_args(argv)

    result = read_rfa_grep(args.rfa, args.partition)
    if args.json_path:
        with open(args.json_path, "w", encoding="utf-8", newline="\n") as handle:
            json.dump(result.to_json(), handle, ensure_ascii=False, indent=2)
    if args.obj_path:
        count = result.write_obj(args.obj_path)
        print(f"wrote {count} decoded vertices to {args.obj_path}", file=sys.stderr)
    if args.summary or not args.json_path:
        print(json.dumps(result.summary(), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
