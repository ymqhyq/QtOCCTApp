import cadquery as cq
def draw(wp, xr, yr, px, nmy, ney, iy):
    return (wp.moveTo(-xr, -yr)
        .threePointArc((-px, 0), (-xr, yr))
        .lineTo(-2, yr)
        .threePointArc((-1.29, nmy), (-1, ney))
        .threePointArc((0, iy), (1, ney))
        .threePointArc((1.29, nmy), (2, yr))
        .lineTo(xr, yr)
        .threePointArc((px, 0), (xr, -yr))
        .lineTo(2, -yr)
        .threePointArc((1.29, -nmy), (1, -ney))
        .threePointArc((0, -iy), (-1, -ney))
        .threePointArc((-1.29, -nmy), (-2, -yr))
        .close())

w = cq.Workplane('XY')
w = draw(w, 16, 14, 30, 13.74, 13, 12)
w = draw(w.workplane(offset=13.75), 17.88, 14.095, 31.905, 13.8, 13.1, 12.1)
w = draw(w.workplane(offset=13.75), 24, 15, 39, 14.71, 14, 13)
tuopan = w.loft()
w = cq.Workplane('XY').workplane(offset=27.5)
w = draw(w, 24, 15, 39, 14.71, 14, 13)
w = draw(w.workplane(offset=2.5), 24, 15, 39, 14.71, 14, 13)
dingmao = w.loft()

cutter = (cq.Workplane('XZ').moveTo(-7.5, 30).lineTo(-7.5, 27)
    .lineTo(-5.5, 25).lineTo(5.5, 25).lineTo(7.5, 27)
    .lineTo(7.5, 30).close().extrude(500, both=True))
tuopan = tuopan.cut(cutter)
dingmao = dingmao.cut(cutter)

w = cq.Workplane('XY').workplane(offset=-120)
w = draw(w, 16, 16.67, 32.67, 16.37, 15.67, 14.67)
w = draw(w.workplane(offset=120), 16, 14, 30, 13.74, 13, 12)
dunshen = w.loft()

ct1 = cq.Workplane('XY').workplane(offset=-125).box(76.82, 44.44, 10)
ct2 = cq.Workplane('XY').workplane(offset=-135).box(89.59, 59.05, 10)
pile = cq.Workplane('XY').circle(5).extrude(60)

assy = cq.Assembly()
assy.add(tuopan)
assy.add(dingmao)
assy.add(dunshen)
assy.add(ct1)
assy.add(ct2)
for xi in [-25, 0, 25]:
    for yi in [-15, 15]:
        assy.add(pile, loc=cq.Location((xi, yi, -200)))

single = assy.toCompound()
result = single.translate((0, 31280, 0))
material = 'plastic'
