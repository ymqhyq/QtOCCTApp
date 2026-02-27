import cadquery as cq
from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections
from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut
from OCP.BRepTools import BRepTools
from OCP.BRepCheck import BRepCheck_Analyzer
import os

L = 5000.0  # Short test length

outer_pts = [
    (0, 0), (2700, 0), (3329, 2515), (6300, 2815),
    (6300, 3032), (-6300, 3032), (-6300, 2815),
    (-3329, 2515), (-2700, 0),
]

inner_1 = [
    (0, 270), (2171, 270), (2471, 570), (2811, 1930),
    (1911, 2430), (-1911, 2430), (-2811, 1930),
    (-2471, 570), (-2171, 270),
]

inner_3 = [
    (0, 600), (1548, 600), (2048, 1100), (2255, 1930),
    (1355, 2460), (-1355, 2460), (-2255, 1930),
    (-2048, 1100), (-1548, 600),
]

def make_wire(pts, z):
    edges = []
    n = len(pts)
    for i in range(n):
        p1 = pts[i]
        p2 = pts[(i + 1) % n]
        edges.append(
            cq.Edge.makeLine(
                cq.Vector(p1[0], p1[1], z),
                cq.Vector(p2[0], p2[1], z),
            )
        )
    return cq.Wire.assembleEdges(edges)

print("Step 1: Creating outer solid...")
outer_solid = cq.Workplane("XY").polyline(outer_pts).close().extrude(L)

print("Step 2: Creating cavity with ThruSections...")
stations = [
    (inner_3, 0.0),
    (inner_1, 1000.0),
    (inner_1, 4000.0),
    (inner_3, 5000.0),
]

cavity_loft = BRepOffsetAPI_ThruSections(True, False)
for pts, z in stations:
    w = make_wire(pts, z)
    cavity_loft.AddWire(w.wrapped)
cavity_loft.Build()
cavity_occ = cavity_loft.Shape()

print(f"  Cavity shape type: {type(cavity_occ)}")
analyzer = BRepCheck_Analyzer(cavity_occ)
print(f"  Cavity is valid: {analyzer.IsValid()}")

print("Step 3: Boolean cut...")
outer_occ = outer_solid.val().wrapped

cut_op = BRepAlgoAPI_Cut(outer_occ, cavity_occ)
cut_op.Build()
print(f"  Cut IsDone: {cut_op.IsDone()}")

result_shape = cut_op.Shape()
print(f"  Result is null: {result_shape.IsNull()}")
analyzer2 = BRepCheck_Analyzer(result_shape)
print(f"  Result is valid: {analyzer2.IsValid()}")

os.makedirs('d:/QtOCCTApp/temp_debug', exist_ok=True)
BRepTools.Write_s(result_shape, "d:/QtOCCTApp/temp_debug/test_cavity.brep")
BRepTools.Write_s(cavity_occ, "d:/QtOCCTApp/temp_debug/test_cavity_only.brep")
BRepTools.Write_s(outer_occ, "d:/QtOCCTApp/temp_debug/test_outer_only.brep")
print("Exported brep files to d:/QtOCCTApp/temp_debug/")
