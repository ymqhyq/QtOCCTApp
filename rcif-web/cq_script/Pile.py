import cadquery as cq

# 钻孔灌注桩 (Pile / SinglePile) - 极简生产版 (V3)
# 移除 .combine() 避免耗时的布尔运算，直接返回形状集合

diameter = globals().get('Diameter', globals().get('diameter', 1000.0))
length = globals().get('Length', globals().get('length', 15000.0))
layout = str(globals().get('Layout', globals().get('layout', "2x3"))).strip()
hSpacing = globals().get('HSpacing', globals().get('hSpacing', 2500.0))
vSpacing = globals().get('VSpacing', 3000.0)

pts = []
if layout == "2x3":
    for xi in [-hSpacing, 0, hSpacing]:
        for yi in [-vSpacing / 2.0, vSpacing / 2.0]:
            pts.append((xi, yi))
elif layout == "3x3":
    for xi in [-hSpacing, 0, hSpacing]:
        for yi in [-vSpacing, 0, vSpacing]:
            pts.append((xi, yi))
else:
    pts = [(0, 0)]

# 生成桩集合，不执行 combine()
result = (cq.Workplane("XY")
          .pushPoints(pts)
          .circle(diameter / 2.0)
          .extrude(-length))

if 'show_object' in globals():
    show_object(result, name="Pile")
