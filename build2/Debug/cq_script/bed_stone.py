import cadquery as cq

# 支承垫石 (BedStone) - 单个构件版本
# 尺寸：1200mm x 1200mm，高度 400mm。

# 定义参数
width = 1200.0
length = 1200.0
height = 400.0
z_min = -50.0

# 创建单个垫石模型 (位于局部坐标系中心)
result = (
    cq.Workplane("XY")
    .workplane(offset=z_min)
    .box(width, length, height, centered=(True, True, False))
)

# 输出
if 'show_object' in globals():
    show_object(result, name="BedStone")
