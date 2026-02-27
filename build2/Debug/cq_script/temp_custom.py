import cadquery as cq

pile = cq.Workplane('XY').circle(500).extrude(6000)
assy = cq.Assembly()
for xi in [-2500, 0, 2500]:
    for yi in [-1500, 1500]:
        assy.add(pile, loc=cq.Location((xi, yi, -(pierHeight + 8000.0))))
result = assy.toCompound()
material = 'brass'
