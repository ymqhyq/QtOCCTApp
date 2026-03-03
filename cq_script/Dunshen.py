import cadquery as cq

def draw(wp, xr, yr, px, nmy, ney, iy):
    return (wp.moveTo(-xr, -yr)
            .threePointArc((-px, 0), (-xr, yr))
            .lineTo(-200, yr)
            .threePointArc((-129, nmy), (-100, ney))
            .threePointArc((0, iy), (100, ney))
            .threePointArc((129, nmy), (200, yr))
            .lineTo(xr, yr)
            .threePointArc((px, 0), (xr, -yr))
            .lineTo(200, -yr)
            .threePointArc((129, -nmy), (100, -ney))
            .threePointArc((0, -iy), (-100, -ney))
            .threePointArc((-129, -nmy), (-200, -yr))
            .close())

# 获取桥墩高度：优先从 schema 的 'height' 获取，其次从全局 'pierHeight' 获取
pierHeight = globals().get('height', globals().get('pierHeight', 12000.0))

w = cq.Workplane('XY').workplane(offset=-pierHeight)
w = draw(w, 1600, 1667, 3267, 1637, 1567, 1467)
w = draw(w.workplane(offset=pierHeight), 1600, 1400, 3000, 1374, 1300, 1200)
result = w.loft()
material = 'plastic'
type = "圆端型"
