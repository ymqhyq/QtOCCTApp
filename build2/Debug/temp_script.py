import cadquery as cq
height = 60.0
width = 80.0
thickness = 10.0
diameter = 22.0

result = (
    cq.Workplane('XY')
    .box(height, width, thickness)
    .faces('>Z')
    .workplane()
    .hole(diameter)
)
