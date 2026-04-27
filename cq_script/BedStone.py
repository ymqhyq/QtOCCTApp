import cadquery as cq

# 支承垫石 (BedStone) 
# 注意：变量名必须与 JSON 中的属性名（大小写）保持一致

# 从 globals() 获取参数，键名需与 Pset_BedStoneGeometry 中的定义一致
# 如果 JSON 中未定义 Width/Length，则使用 1200mm 作为默认值
width = globals().get('Width', 1200.0)
length = globals().get('Length', 1200.0)
height = globals().get('Height', 400.0)

# 建模逻辑
# centered=(True, True, False) 意味着 (0,0,0) 在底面中心
result = (
    cq.Workplane("XY")
    .box(width, length, height, centered=(True, True, False))
)

# 调试输出
if 'show_object' in globals():
    show_object(result, name="BedStone")
