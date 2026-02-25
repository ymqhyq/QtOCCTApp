import cadquery as cq

pile = cq.Workplane('XY').circle(5).extrude(60)
assy = cq.Assembly()
for xi in [-25, 0, 25]:
    for yi in [-15, 15]:
        assy.add(pile, loc=cq.Location((xi, yi, -(pierHeight + 80.0))))
result = assy.toCompound()
material = 'brass'
