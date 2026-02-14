import cadquery as cq
# Create a simple box
result = cq.Workplane('XY').box(100, 100, 100).edges().chamfer(10)
# result variable is automatically exported
