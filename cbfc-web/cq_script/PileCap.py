import cadquery as cq

# 输入参数处理
L = globals().get('Length', 7000.0)
W = globals().get('Width', 5000.0)
H = globals().get('Height', 2000.0)
layers = globals().get('Layers', 1)

# 建模逻辑
# 将承台中心设在 (0,0,0)，厚度向上增加。
ct1 = cq.Workplane('XY').box(L, W, H, centered=(True, True, False))
result_shape = ct1

if layers > 1:
    ct2 = cq.Workplane('XY').workplane(offset=H).box(L*1.2, W*1.2, H, centered=(True, True, False))
    result_shape = result_shape.union(ct2)

result = result_shape

# 计算输出参数
totalH = result.val().Volume() / 1e9 # 转换为立方米 (从立方毫米)

# 材质设置
material = 'stone'
