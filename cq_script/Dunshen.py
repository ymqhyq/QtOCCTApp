import cadquery as cq

def draw(wp, xr, yr, px, nmy, ney, iy):
    return (wp.moveTo(-xr, -yr)
            .threePointArc((-px, 0), (-xr, yr))
            .lineTo(-2, yr)
            .threePointArc((-1.29, nmy), (-1, ney))
            .threePointArc((0, iy), (1, ney))
            .threePointArc((1.29, nmy), (2, yr))
            .lineTo(xr, yr)
            .threePointArc((px, 0), (xr, -yr))
            .lineTo(2, -yr)
            .threePointArc((1.29, -nmy), (1, -ney))
            .threePointArc((0, -iy), (-1, -ney))
            .threePointArc((-1.29, -nmy), (-2, -yr))
            .close())

w = cq.Workplane('XY').workplane(offset=-pierHeight)
w = draw(w, 16, 16.67, 32.67, 16.37, 15.67, 14.67)
w = draw(w.workplane(offset=pierHeight), 16, 14, 30, 13.74, 13, 12)
result = w.loft()
material = 'plastic'
