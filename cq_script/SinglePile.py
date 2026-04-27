import cadquery as cq

# 钻孔灌注桩 (SinglePile) - 标准化版本
# 变量名与 JSON 中的 Pset_SinglePileGeometry 保持一致 (首字母大写)

diameter = globals().get('Diameter', 1000.0)
length = globals().get('Length', 15000.0)
layout = globals().get('Layout', "2x3")
hSpacing = globals().get('HSpacing', 2500.0)
vSpacing = globals().get('VSpacing', 3000.0)

# 创建单根桩模型 (在 Z=0 处向下挤压)
pile = cq.Workplane('XY').circle(diameter / 2.0).extrude(-length)
assy = cq.Assembly()

# 布局逻辑
if layout == "2x3":
    xs = [-hSpacing, 0, hSpacing]
    ys = [-vSpacing / 2.0, vSpacing / 2.0]
elif layout == "3x3":
    xs = [-hSpacing, 0, hSpacing]
    ys = [-vSpacing, 0, vSpacing]
elif layout == "4x4":
    xs = [-hSpacing * 1.5, -hSpacing * 0.5, hSpacing * 0.5, hSpacing * 1.5]
    ys = [-vSpacing * 1.5, -vSpacing * 0.5, vSpacing * 0.5, vSpacing * 1.5]
else:
    xs = [0]
    ys = [0]

# 将桩添加到组中
# 注意：不再在 loc 中添加 pierHeight 等偏移，位姿由装配树决定
for xi in xs:
    for yi in ys:
        assy.add(pile, loc=cq.Location((xi, yi, 0)))

result = assy.toCompound()

if 'show_object' in globals():
    show_object(result, name="SinglePile")
