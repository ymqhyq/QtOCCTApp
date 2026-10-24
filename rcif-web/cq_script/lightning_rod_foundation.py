import cadquery as cq

# 1. 定义基本参数 (单位: mm)
# 底座 (由两层组成：-2500到-2400, -2400到-1500)
base_width = 2700.0
base_height_1 = 100.0
base_height_2 = 900.0
base_total_height = base_height_1 + base_height_2 # 总高 1000

# 中段 (-1500到-700)
mid_width = 2200.0
mid_height = 800.0

# 顶部四个立柱 (-700到200)
post_width = 500.0
post_height = 900.0
post_offset = 647.5 # 立柱中心到坐标轴的距离

# 2. 构建几何体

# 第一步：构建底座 (从 Z = -2500 开始向上拉伸)
model = (
    cq.Workplane("XY")
    .workplane(offset=-2500.0)
    .box(base_width, base_width, base_total_height, centered=(True, True, False))
)

# 第二步：构建中段 (在底座之上)
model = (
    model.faces(">Z")
    .workplane()
    .box(mid_width, mid_width, mid_height, centered=(True, True, False))
)

# 第三步：构建四个对称的立柱
post_centers = [
    (post_offset, post_offset),
    (post_offset, -post_offset),
    (-post_offset, post_offset),
    (-post_offset, -post_offset)
]

result = (
    model.faces(">Z")
    .workplane()
    .pushPoints(post_centers)
    .box(post_width, post_width, post_height, centered=(True, True, False))
)

# 3. 输出模型
if 'show_object' in globals():
    show_object(result, name="LightningRodFoundation")
