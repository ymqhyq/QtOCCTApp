import cadquery as cq

ct1 = cq.Workplane('XY').workplane(offset=-(pierHeight + 5.0)).box(76.82, 44.44, 10)
ct2 = cq.Workplane('XY').workplane(offset=-(pierHeight + 15.0)).box(89.59, 59.05, 10)
assy = cq.Assembly()
assy.add(ct1)
assy.add(ct2)
result = assy.toCompound()
material = 'chrome'
