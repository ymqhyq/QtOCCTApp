import cadquery as cq

def draw(wp, xr, yr, px, nmy, ney, iy):
    return (wp.moveTo(-xr, -yr)
            .threePointArc((-px, 0), (-xr, yr))
            .lineTo(-200, yr)
            .threePointArc((-129, nmy), (-100, ney))
            .threePointArc((0, iy), (100, ney))
            .threePointArc((129, nmy), (200, yr))
            .lineTo(xr, yr)
            .threePointArc((px, 0), (xr, -yr))
            .lineTo(200, -yr)
            .threePointArc((129, -nmy), (100, -ney))
            .threePointArc((0, -iy), (-100, -ney))
            .threePointArc((-129, -nmy), (-200, -yr))
            .close())

# 单位换算：由于 C++ 已经发送了毫米值的 pierHeight 和 yOffset，这里直接使用即可
pierHeight = globals().get('pierHeight', 12000.0)
yOffset_val = globals().get('yOffset', 0.0)

w = cq.Workplane('XY')
w = draw(w, 1600, 1400, 3000, 1374, 1300, 1200)
w = draw(w.workplane(offset=1375), 1788, 1409.5, 3190.5, 1380, 1310, 1210)
w = draw(w.workplane(offset=1375), 2400, 1500, 3900, 1471, 1400, 1300)
tuopan = w.loft()

w = cq.Workplane('XY').workplane(offset=2750)
w = draw(w, 2400, 1500, 3900, 1471, 1400, 1300)
w = draw(w.workplane(offset=250), 2400, 1500, 3900, 1471, 1400, 1300)
dingmao = w.loft()

cutter = (cq.Workplane('XZ').moveTo(-750, 3000).lineTo(-750, 2700)
          .lineTo(-550, 2500).lineTo(550, 2500).lineTo(750, 2700)
          .lineTo(750, 3000).close().extrude(50000, both=True))
tuopan = tuopan.cut(cutter)
dingmao = dingmao.cut(cutter)

w = cq.Workplane('XY').workplane(offset=-pierHeight)
w = draw(w, 1600, 1667, 3267, 1637, 1567, 1467)
w = draw(w.workplane(offset=pierHeight), 1600, 1400, 3000, 1374, 1300, 1200)
dunshen = w.loft()

ct1 = cq.Workplane('XY').workplane(offset=-(pierHeight + 500.0)).box(7682, 4444, 1000)
ct2 = cq.Workplane('XY').workplane(offset=-(pierHeight + 1500.0)).box(8959, 5905, 1000)
pile = cq.Workplane('XY').circle(500).extrude(6000)

assy = cq.Assembly()
assy.add(tuopan)
assy.add(dingmao)
assy.add(dunshen)
assy.add(ct1)
assy.add(ct2)
for xi in [-2500, 0, 2500]:
    for yi in [-1500, 1500]:
        assy.add(pile, loc=cq.Location((xi, yi, -(pierHeight + 8000.0))))

single = assy.toCompound()
result = single.translate((0, yOffset_val, 0))
material = 'plastic'
