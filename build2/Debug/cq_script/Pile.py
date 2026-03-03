import cadquery as cq

pierHeight = globals().get('pierHeight', 12000.0)
diameter = globals().get('diameter', 1000.0)
length = globals().get('length', 15000.0)
layout = globals().get('layout', "2x3")
hSpacing = globals().get('hSpacing', 2500.0)
vSpacing = globals().get('vSpacing', 3000.0)

pile = cq.Workplane('XY').circle(diameter / 2.0).extrude(length)
assy = cq.Assembly()

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
    xs = [-hSpacing, 0, hSpacing]
    ys = [-vSpacing / 2.0, vSpacing / 2.0]

for xi in xs:
    for yi in ys:
        assy.add(pile, loc=cq.Location((xi, yi, -(pierHeight + 8000.0))))

result = assy.toCompound()
material = 'brass'

if 'show_object' in globals():
    show_object(result, name="Pile")

