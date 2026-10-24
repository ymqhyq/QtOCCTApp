"""Build a valid AP242 STEP shell from a triangle mesh.

The web exporter uses this for non-CadQuery components. The previous
hand-written POLY_LOOP/FACETED_BREP writer produced a syntactically valid file
that CAD readers could not rebuild into a shell. Sewing OCC faces here gives
the STEP reader real shared edges and topology.
"""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

from OCP.BRepBuilderAPI import (
    BRepBuilderAPI_MakeFace,
    BRepBuilderAPI_MakePolygon,
    BRepBuilderAPI_Sewing,
)
from OCP.STEPControl import STEPControl_StepModelType, STEPControl_Writer
from OCP.gp import gp_Pnt


def build_shell(payload: dict) -> object:
    positions = payload["positions"]
    indices = payload["indices"]
    sewing = BRepBuilderAPI_Sewing(1.0e-3)
    for offset in range(0, len(indices), 3):
        polygon = BRepBuilderAPI_MakePolygon()
        for vertex in indices[offset : offset + 3]:
            base = int(vertex) * 3
            polygon.Add(gp_Pnt(float(positions[base]), float(positions[base + 1]), float(positions[base + 2])))
        polygon.Close()
        if not polygon.IsDone():
            continue
        face = BRepBuilderAPI_MakeFace(polygon.Wire())
        if face.IsDone():
            sewing.Add(face.Face())
    sewing.Perform()
    shape = sewing.SewedShape()
    if shape.IsNull():
        raise RuntimeError("三角网格无法缝合为壳体")
    return shape


def main() -> int:
    payload = json.loads(sys.stdin.buffer.read().decode("utf-8"))
    shape = build_shell(payload)
    with tempfile.NamedTemporaryFile(suffix=".step", delete=False) as handle:
        output = Path(handle.name)
    try:
        writer = STEPControl_Writer()
        writer.Transfer(shape, STEPControl_StepModelType.STEPControl_AsIs)
        if not writer.Write(str(output)):
            raise RuntimeError("STEP 写入失败")
        data = output.read_bytes()
        marker = b"ISO-10303-21;"
        start = data.find(marker)
        sys.stdout.buffer.write(data[start:] if start >= 0 else data)
    finally:
        output.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
