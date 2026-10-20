import cadquery as cq

# 支座 (Bearing)
# 根据对 53-12 20 00_GZQT-3-01_8.000.ifc 的分析：
# 该支座位于垫石上方 (Z从 350 到 600)。
# 结构包含：底钢板、圆柱体支座芯、顶钢板。

# 定义参数 (均为 out 参数，由脚本内部计算输出)
type = "固定支座"
height = 250.0
length = 650.0
width = 650.0

# 内部衍生参数 (单位: mm)
base_dim = length    # 钢板尺寸使用 length 控制
plate_h = 20.0       # 钢板厚度
core_r = 241.25      # 圆柱芯半径
core_h = height - 2 * plate_h # 总高减去两块钢板厚度

# 创建模型
# 1. 创建底钢板
result = (
    cq.Workplane("XY")
    .box(base_dim, base_dim, plate_h, centered=(True, True, False))
)

# 2. 在底板上创建圆柱芯
result = (
    result.faces(">Z")
    .workplane()
    .circle(core_r)
    .extrude(core_h)
)

# 3. 在圆柱芯上创建顶钢板
result = (
    result.faces(">Z")
    .workplane()
    .box(base_dim, base_dim, plate_h, centered=(True, True, False))
)

# 输出
if 'show_object' in globals():
    show_object(result, name="Bearing")
