# 流线型桥墩 - CadQuery/OCP 脚本
from OCP.gp import gp_Pnt, gp_Dir, gp_Pln
from OCP.GC import GC_MakeArcOfCircle, GC_MakeSegment
from OCP.BRepBuilderAPI import (
    BRepBuilderAPI_MakeEdge, BRepBuilderAPI_MakeWire,
    BRepBuilderAPI_MakeFace)
from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepFeat import BRepFeat_MakePrism
from OCP.BRepLib import BRepLib
from OCP.BRep import BRep_Builder
from OCP.TopoDS import TopoDS_Compound

def make_wire(P1, P2, P3, P1_1, P2_1, P3_1,
              P1_2, P2_2, P3_2, P1_3, P2_3, P3_3, P3_4,
              P1_5, P2_5, P3_5, P1_6, P2_6, P3_6, P3_7):
    from OCP.GC import GC_MakeArcOfCircle, GC_MakeSegment
    from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeEdge, BRepBuilderAPI_MakeWire
    edges = [
        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P1_1, P2_1, P3_1).Value()).Edge(),
        BRepBuilderAPI_MakeEdge(GC_MakeSegment(P3_1, P1_6).Value()).Edge(),
        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P1_6, P2_6, P3_6).Value()).Edge(),
        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P3_6, P3_7, P3_5).Value()).Edge(),
        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P3_5, P2_5, P1_5).Value()).Edge(),
        BRepBuilderAPI_MakeEdge(GC_MakeSegment(P1_5, P3).Value()).Edge(),
        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P3, P2, P1).Value()).Edge(),
        BRepBuilderAPI_MakeEdge(GC_MakeSegment(P1, P1_2).Value()).Edge(),
        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P1_2, P2_2, P3_2).Value()).Edge(),
        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P3_2, P3_4, P3_3).Value()).Edge(),
        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P3_3, P2_3, P1_3).Value()).Edge(),
        BRepBuilderAPI_MakeEdge(GC_MakeSegment(P1_3, P1_1).Value()).Edge(),
    ]
    mw = BRepBuilderAPI_MakeWire()
    for e in edges:
        mw.Add(e)
    return mw.Wire()

def section(z, xr, yr, xr2, yr2, r_inner):
    """Generate section points at height z"""
    return make_wire(
        gp_Pnt(xr, -yr, z), gp_Pnt(xr+xr2-xr, 0, z), gp_Pnt(xr, yr, z),
        gp_Pnt(-xr, -yr, z), gp_Pnt(-(xr+xr2-xr), 0, z), gp_Pnt(-xr, yr, z),
        gp_Pnt(2, -yr, z), gp_Pnt(1.29, -(yr-0.26), z), gp_Pnt(1, -(yr-1), z),
        gp_Pnt(-2, -yr, z), gp_Pnt(-1.29, -(yr-0.26), z), gp_Pnt(-1, -(yr-1), z),
        gp_Pnt(0, -(yr-r_inner), z),
        gp_Pnt(2, yr, z), gp_Pnt(1.29, yr-0.26, z), gp_Pnt(1, yr-1, z),
        gp_Pnt(-2, yr, z), gp_Pnt(-1.29, yr-0.26, z), gp_Pnt(-1, yr-1, z),
        gp_Pnt(0, yr-r_inner, z))

# 截面1: Z=0 (托盘底部)
aWire = make_wire(
    gp_Pnt(16,-14,0), gp_Pnt(30,0,0), gp_Pnt(16,14,0),
    gp_Pnt(-16,-14,0), gp_Pnt(-30,0,0), gp_Pnt(-16,14,0),
    gp_Pnt(2,-14,0), gp_Pnt(1.29,-13.74,0), gp_Pnt(1,-13,0),
    gp_Pnt(-2,-14,0), gp_Pnt(-1.29,-13.74,0), gp_Pnt(-1,-13,0),
    gp_Pnt(0,-12,0),
    gp_Pnt(2,14,0), gp_Pnt(1.29,13.74,0), gp_Pnt(1,13,0),
    gp_Pnt(-2,14,0), gp_Pnt(-1.29,13.74,0), gp_Pnt(-1,13,0),
    gp_Pnt(0,12,0))

# 截面2: Z=27.5 (托盘顶部)
bWire = make_wire(
    gp_Pnt(24,-15,27.5), gp_Pnt(39,0,27.5), gp_Pnt(24,15,27.5),
    gp_Pnt(-24,-15,27.5), gp_Pnt(-39,0,27.5), gp_Pnt(-24,15,27.5),
    gp_Pnt(2,-15,27.5), gp_Pnt(1.29,-14.71,27.5), gp_Pnt(1,-14,27.5),
    gp_Pnt(-2,-15,27.5), gp_Pnt(-1.29,-14.71,27.5), gp_Pnt(-1,-14,27.5),
    gp_Pnt(0,-13,27.5),
    gp_Pnt(2,15,27.5), gp_Pnt(1.29,14.71,27.5), gp_Pnt(1,14,27.5),
    gp_Pnt(-2,15,27.5), gp_Pnt(-1.29,14.71,27.5), gp_Pnt(-1,14,27.5),
    gp_Pnt(0,13,27.5))

# 截面3: Z=13.75 (中间过渡)
mWire = make_wire(
    gp_Pnt(17.88,-14.095,13.75), gp_Pnt(31.905,0,13.75), gp_Pnt(17.88,14.095,13.75),
    gp_Pnt(-17.88,-14.095,13.75), gp_Pnt(-31.905,0,13.75), gp_Pnt(-17.88,14.095,13.75),
    gp_Pnt(2,-14.095,13.75), gp_Pnt(1.29,-13.8,13.75), gp_Pnt(1,-13.1,13.75),
    gp_Pnt(-2,-14.095,13.75), gp_Pnt(-1.29,-13.8,13.75), gp_Pnt(-1,-13.1,13.75),
    gp_Pnt(0,-12.1,13.75),
    gp_Pnt(2,14.095,13.75), gp_Pnt(1.29,13.8,13.75), gp_Pnt(1,13.1,13.75),
    gp_Pnt(-2,14.095,13.75), gp_Pnt(-1.29,13.8,13.75), gp_Pnt(-1,13.1,13.75),
    gp_Pnt(0,12.1,13.75))

# 截面4: Z=30 (顶帽)
dWire = make_wire(
    gp_Pnt(24,-15,30), gp_Pnt(39,0,30), gp_Pnt(24,15,30),
    gp_Pnt(-24,-15,30), gp_Pnt(-39,0,30), gp_Pnt(-24,15,30),
    gp_Pnt(2,-15,30), gp_Pnt(1.29,-14.71,30), gp_Pnt(1,-14,30),
    gp_Pnt(-2,-15,30), gp_Pnt(-1.29,-14.71,30), gp_Pnt(-1,-14,30),
    gp_Pnt(0,-13,30),
    gp_Pnt(2,15,30), gp_Pnt(1.29,14.71,30), gp_Pnt(1,14,30),
    gp_Pnt(-2,15,30), gp_Pnt(-1.29,14.71,30), gp_Pnt(-1,14,30),
    gp_Pnt(0,13,30))

# 截面5: Z=-120 (墩身底部)
sWire = make_wire(
    gp_Pnt(16,-16.67,-120), gp_Pnt(32.67,0,-120), gp_Pnt(16,16.67,-120),
    gp_Pnt(-16,-16.67,-120), gp_Pnt(-32.67,0,-120), gp_Pnt(-16,16.67,-120),
    gp_Pnt(2,-16.67,-120), gp_Pnt(1.29,-16.37,-120), gp_Pnt(1,-15.67,-120),
    gp_Pnt(-2,-16.67,-120), gp_Pnt(-1.29,-16.37,-120), gp_Pnt(-1,-15.67,-120),
    gp_Pnt(0,-14.67,-120),
    gp_Pnt(2,16.67,-120), gp_Pnt(1.29,16.37,-120), gp_Pnt(1,15.67,-120),
    gp_Pnt(-2,16.67,-120), gp_Pnt(-1.29,16.37,-120), gp_Pnt(-1,15.67,-120),
    gp_Pnt(0,14.67,-120))

# === 流线型托盘 (ThruSections) ===
from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections
tuopan = BRepOffsetAPI_ThruSections(True, False)
tuopan.AddWire(aWire)
tuopan.AddWire(mWire)
tuopan.AddWire(bWire)
tuopan.Build()
S = tuopan.Shape()

# === 顶帽 ===
dingmao = BRepOffsetAPI_ThruSections(True, False)
dingmao.AddWire(bWire)
dingmao.AddWire(dWire)
dingmao.Build()
S1 = dingmao.Shape()

# === 裁剪 (Prism切割) ===
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeEdge, BRepBuilderAPI_MakeWire, BRepBuilderAPI_MakeFace
from OCP.BRepFeat import BRepFeat_MakePrism
from OCP.BRepLib import BRepLib
from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRep import BRep_Builder
from OCP.TopoDS import TopoDS_Compound
D = gp_Dir(0, 1, 0)
MW = BRepBuilderAPI_MakeWire()
pts = [(-7.5,-100,30), (-7.5,-100,27), (-5.5,-100,25),
       (5.5,-100,25), (7.5,-100,27), (7.5,-100,30), (-7.5,-100,30)]
for i in range(len(pts)-1):
    MW.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(*pts[i]), gp_Pnt(*pts[i+1])).Edge())

zox_pln = gp_Pln(gp_Pnt(0,0,0), gp_Dir(0,1,0))
FP = BRepBuilderAPI_MakeFace(zox_pln, MW.Wire()).Shape()
BRepLib.BuildCurves3d_s(FP)
F = BRepBuilderAPI_MakeFace(zox_pln).Face()

MKP = BRepFeat_MakePrism(S, FP, F, D, 0, True)
MKP.Perform(1000.0)
res = MKP.Shape()

MKP1 = BRepFeat_MakePrism(S1, FP, F, D, 0, True)
MKP1.Perform(1000.0)
res1 = MKP1.Shape()

# === 墩身 ===
dunshen = BRepOffsetAPI_ThruSections(True, False)
dunshen.AddWire(sWire)
dunshen.AddWire(aWire)
dunshen.Build()
S2 = dunshen.Shape()

# === 承台 (两层底座) ===
S3 = BRepPrimAPI_MakeBox(gp_Pnt(-38.41,-22.22,-130), 76.82, 44.44, 10).Shape()
S4 = BRepPrimAPI_MakeBox(gp_Pnt(-44.79,-29.53,-140), 89.59, 59.05, 10).Shape()

# === 组合为 Compound ===
builder = BRep_Builder()
compound = TopoDS_Compound()
builder.MakeCompound(compound)
for s in [res, res1, S2, S3, S4]:
    builder.Add(compound, s)

result = compound
material = 'plastic'
