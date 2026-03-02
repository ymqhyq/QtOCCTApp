import cadquery as cq

# 桩基础：3x2 阵列
pile_r = 750.0
pile_l = 10000.0
x_coords = [-3500.0, 0.0, 3500.0]
y_coords = [-2000.0, 2000.0]

result = cq.Workplane("XY")
for x in x_coords:
    for y in y_coords:
        result = result.union(
            cq.Workplane("XY")
            .workplane(offset=-12000.0 - 2500.0 - pile_l) # bodyBottomZ - footingH - pileL
            .center(x, y)
            .cylinder(pile_l, pile_r, centered=(True, True, False))
        )

if "show_object" in locals():
    show_object(result)
