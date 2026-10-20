# 流线型桥墩 - CadQuery Workplane 链式语法
import cadquery as cq

def draw(wp, xr, yr, px, nmy, ney, iy):
    """绘制桥墩截面轮廓"""
    return (wp
        .moveTo(-xr, -yr)
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

# === 流线型托盘 (Z=0 -> 13.75 -> 27.5) ===
w = cq.Workplane('XY')
w = draw(w, 1600, 1400, 3000, 1374, 1300, 1200)
w = draw(w.workplane(offset=1375),
         1788, 1409.5, 3190.5, 1380, 1310, 1210)
w = draw(w.workplane(offset=1375),
         2400, 1500, 3900, 1471, 1400, 1300)
tuopan = w.loft()

# === 顶帽 (Z=27.5 -> 30) ===
w = cq.Workplane('XY').workplane(offset=2750)
w = draw(w, 2400, 1500, 3900, 1471, 1400, 1300)
w = draw(w.workplane(offset=250),
         2400, 1500, 3900, 1471, 1400, 1300)
dingmao = w.loft()

# === 裁剪 (梯形槽道沿Y方向) ===
cutter = (cq.Workplane('XZ')
    .moveTo(-750, 3000).lineTo(-750, 2700)
    .lineTo(-550, 2500).lineTo(550, 2500)
    .lineTo(750, 2700).lineTo(750, 3000)
    .close().extrude(50000, both=True))
tuopan = tuopan.cut(cutter)
dingmao = dingmao.cut(cutter)

pierHeight = globals().get('pierHeight', 12000.0)
# === 墩身 ===
w = cq.Workplane('XY').workplane(offset=-pierHeight)
w = draw(w, 1600, 1667, 3267, 1637, 1567, 1467)
w = draw(w.workplane(offset=pierHeight),
         1600, 1400, 3000, 1374, 1300, 1200)
dunshen = w.loft()

# === 承台 (两层底座) ===
ct1 = cq.Workplane('XY').workplane(offset=-(pierHeight + 500)).box(7682, 4444, 1000)
ct2 = cq.Workplane('XY').workplane(offset=-(pierHeight + 1500)).box(8959, 5905, 1000)
# === Assembly 装配 (桩共享同一几何) ===
pile = cq.Workplane('XY').circle(500).extrude(-6000)
assy = cq.Assembly()
assy.add(tuopan, name='tuopan')
assy.add(dingmao, name='dingmao')
assy.add(dunshen, name='dunshen')
assy.add(ct1, name='ct1')
assy.add(ct2, name='ct2')
for xi in [-2500, 0, 2500]:
    for yi in [-1500, 1500]:
        assy.add(pile,
            loc=cq.Location((xi, yi, -(pierHeight + 2000))),
            name=f'pile_{xi}_{yi}')
result = assy.toCompound()
material = 'plastic'
