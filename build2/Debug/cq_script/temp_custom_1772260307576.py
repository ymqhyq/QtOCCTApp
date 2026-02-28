import cadquery as cq

ct1 = cq.Workplane('XY').workplane(offset=-(pierHeight + 500.0)).box(7682, 4444, 1000)
ct2 = cq.Workplane('XY').workplane(offset=-(pierHeight + 1500.0)).box(8959, 5905, 1000)
assy = cq.Assembly()
assy.add(ct1)
assy.add(ct2)
result = assy.toCompound()
material = 'chrome'
