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

height = globals().get('Height', 2750.0)
length = 3900.0
width = 2400.0
concreteGrade = "C50"

w = cq.Workplane('XY')
w = draw(w, 1600, 1400, 3000, 1374, 1300, 1200)
w = draw(w.workplane(offset=height/2.0), 1788, 1409.5, 3190.5, 1380, 1310, 1210)
w = draw(w.workplane(offset=height/2.0), 2400, 1500, 3900, 1471, 1400, 1300)
tuopan = w.loft()

w = cq.Workplane('XY').workplane(offset=height)
w = draw(w, 2400, 1500, 3900, 1471, 1400, 1300)
w = draw(w.workplane(offset=250), 2400, 1500, 3900, 1471, 1400, 1300)
dingmao = w.loft()

cutter = (cq.Workplane('XZ').moveTo(-750, height + 250).lineTo(-750, height - 50)
          .lineTo(-550, height - 250).lineTo(550, height - 250).lineTo(750, height - 50)
          .lineTo(750, height + 250).close().extrude(50000, both=True))
tuopan = tuopan.cut(cutter)
dingmao = dingmao.cut(cutter)

assy = cq.Assembly()
assy.add(tuopan)
assy.add(dingmao)
result = assy.toCompound()
material = 'steel'
