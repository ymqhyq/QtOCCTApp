import cadquery as cq

# ============================================================
# 变截面箱梁建模脚本 (纯 CadQuery 实现 - 稳定版)
# 三个截面: I-I (跨中), II-II (过渡), III-III (梁端)
# 纵向布置: 梁端1300mm恒定III -> 854mm过渡到II -> 1646mm过渡到I
# ============================================================

# === 参数处理 ===
type = globals().get('type', "单箱单室")
length = globals().get('length', 31500.0)  # 梁总长 (inout)
width = 12600.0   # 顶板全宽 (out)
height = 3032.0   # 梁高 (out)

L = length

# === 外轮廓 (所有截面恒定不变) ===
outer_pts = [
    (0, 0),
    (2700, 0),
    (3329, 2515),
    (6300, 2815),
    (6300, 3032),
    (-6300, 3032),
    (-6300, 2815),
    (-3329, 2515),
    (-2700, 0),
]

# === 三个截面的内腔轮廓 (各9个顶点, 拓扑一致) ===

# 截面 I-I (跨中): 底板270, 下梗腋300x300, 腹板1360, 上梗腋500x900
inner_1 = [
    (0, 270), (2171, 270), (2471, 570), (2811, 1930), (1911, 2430),
    (-1911, 2430), (-2811, 1930), (-2471, 570), (-2171, 270)
]

# 截面 II-II (过渡): 底板487, 下梗腋432x432, 腹板1011, 上梗腋622x716
inner_2 = [
    (0, 487), (1760, 487), (2192, 919), (2445, 1930), (1729, 2552),
    (-1729, 2552), (-2445, 1930), (-2192, 919), (-1760, 487)
]

# 截面 III-III (梁端): 底板600, 下梗腋500x500, 腹板830, 上梗腋530x900
inner_3 = [
    (0, 600), (1548, 600), (2048, 1100), (2255, 1930), (1355, 2460),
    (-1355, 2460), (-2255, 1930), (-2048, 1100), (-1548, 600)
]

# === 纵向站点定义 (从 z=0 梁端到 z=L 梁端) ===
# 定义关键站点的 Z 坐标
z0 = 0.0
z1 = 1300.0
z2 = 2154.0     # 1300 + 854
z3 = 3800.0     # 2154 + 1646
z4 = L - 3800.0
z5 = L - 2154.0
z6 = L - 1300.0
z7 = L

# === 1. 创建外壳实体 ===
outer_solid = cq.Workplane("YZ").polyline(outer_pts).close().extrude(L)

class OuterR300Selector(cq.Selector):
    def filter(self, objectList):
        res = []
        for e in objectList:
            c = e.Center()
            length_x = e.BoundingBox().xlen
            if length_x < L * 0.9: continue
            # 底部外角: Y 约等于 +-2700, Z 约等于 0
            if abs(c.z) < 10 and abs(abs(c.y) - 2700) < 10: 
                res.append(e)
                continue
            # 悬臂下方角: Y 约等于 +-3329, Z 约等于 2515
            if abs(c.z - 2515) < 10 and abs(abs(c.y) - 3329) < 10: 
                res.append(e)
                continue
        return res

outer_edges = outer_solid.edges(OuterR300Selector())
if len(outer_edges.vals()) > 0:
    outer_solid = outer_edges.fillet(300)

# === 2. 创建贯通内腔 (采用相对偏移构建工作平面，提高放样稳定性) ===
# 初始站点的坐标是 x = -10.0 (为了布尔减完全切通)
stations_rel = [
    (inner_3, 10.0),    # 第1步：从 -10 偏移到 0 (10mm)
    (inner_3, 1300.0),  # 第2步：从 0 到 1300
    (inner_2, 854.0),   # 第3步：从 1300 到 2154
    (inner_1, 1646.0),  # 第4步：从 2154 到 3800
    (inner_1, L - 7600.0), # 第5步：跨中恒定段从 3800 到 23900+3800=27700 (偏移 23900)
    (inner_2, 1646.0),  # 第6步：从 27700 到 29346
    (inner_3, 854.0),   # 第7步：从 29346 到 30200
    (inner_3, 1300.0),  # 第8步：从 30200 到 31500
    (inner_3, 10.0)     # 第9步：从 31500 偏移到 31510 (10mm 延长)
]

# 在 x = -10.0 处绘制第一个截面线框
cavity_wp = cq.Workplane("YZ").workplane(offset=-10.0).polyline(inner_3).close()

# 逐个添加相对偏移的工作平面并绘制截面
for pts, rel_dist in stations_rel:
    cavity_wp = cavity_wp.workplane(offset=rel_dist).polyline(pts).close()

# 调用 loft(ruled=True) 生成贯通内腔实体
cavity_solid = cavity_wp.loft(ruled=True)

class InnerR50Selector(cq.Selector):
    def filter(self, objectList):
        res = []
        for e in objectList:
            c = e.Center()
            length_x = e.BoundingBox().xlen
            if length_x < 1.0: continue
            # 顶部内拐角 (R50): 位于两侧，Y的绝对值在 1000~2000 之间，Z 高于 2400
            if c.z > 2400 and 1000 < abs(c.y) < 2000: 
                res.append(e)
        return res

cavity_edges = cavity_solid.edges(InnerR50Selector())
if len(cavity_edges.vals()) > 0:
    cavity_solid = cavity_edges.fillet(50)

# === 3. 布尔减：从外壳中挖去内腔 ===
result = outer_solid.cut(cavity_solid)

# === 4. 可视化输出 ===
if 'show_object' in globals():
    show_object(result, name="Variable_Girder", options={"color": (150, 150, 150)})
