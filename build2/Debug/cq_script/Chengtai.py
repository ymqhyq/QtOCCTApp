import cadquery as cq

# 输入参数处理
pierHeight = globals().get('pierHeight', 12000.0)
layers = globals().get('layers', 2)

# 建模逻辑
ct1 = cq.Workplane('XY').workplane(offset=-(pierHeight + 500.0)).box(7682, 4444, 1000)
result_shape = ct1

if layers > 1:
    ct2 = cq.Workplane('XY').workplane(offset=-(pierHeight + 1500.0)).box(8959, 5905, 1000)
    result_shape = result_shape.union(ct2)

result = result_shape

# 计算输出参数
totalH = result.val().Volume() / 1e9 # 转换为立方米 (从立方毫米)

# 材质设置
material = 'stone'
