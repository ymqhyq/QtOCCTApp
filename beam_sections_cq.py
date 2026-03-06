"""
梁截面 CadQuery 建模脚本
包含三个截面：I-I（跨中截面）、II-II（支座附近截面）、III-III（支座截面）

根据图纸提取的详细尺寸：
- 截面 I-I：跨中截面，全宽 12600mm，梁高 3032mm
- 截面 II-II：支座附近截面，全宽 12600mm，梁高 3015mm  
- 截面 III-III：支座截面，全宽 12600mm，梁高 3032mm

坐标系：X 轴为梁纵向，Y 轴水平向右，Z 轴垂直向上
"""

import cadquery as cq
import math


def create_section_I_I(extrude_length=200):
    """
    创建截面 I-I（跨中截面）
    
    尺寸参数（单位：mm）：
    - 全宽：12600（半宽 6300）
    - 梁高：3032
    - 底板厚：300
    - 底板内宽：2171×2 = 4342
    - 顶板中心厚：285
    - 顶板内半宽：1911
    - 腹板厚：360
    - 腹板直段高：1350
    - 腹板水平投影：629
    - 下承托：R300
    - 上承托：R50
    """
    # ========== 外轮廓（完整截面，逆时针方向） ==========
    outer = cq.Workplane("XY").moveTo(-6300, 0)
    
    # 左悬臂端底部
    # 底板左段（到 R300 起点）
    # R300 圆心：(-3000, 300)，起点 (-3000, 0)，终点 (-2700, 300)
    outer = outer.lineTo(-3000, 0)
    outer = outer.threePointArc((-3000, 300), (-2700, 300))
    
    # 左腹板外边（斜度 4:1）
    # 从 (-2700, 300) 到 (-3629, 2815)
    # 水平投影：629，垂直：2515
    outer = outer.lineTo(-3629, 2815)
    
    # 左腹板顶部垂直段（500mm 高）
    outer = outer.lineTo(-3629, 2815 + 500)
    
    # 左悬臂根部
    outer = outer.lineTo(-6300, 2732)  # 2815+500-217 = 3098，但图上是 2732
    outer = outer.lineTo(-6300, 3032)
    
    # 顶板上边缘
    outer = outer.lineTo(6300, 3032)
    
    # 右悬臂
    outer = outer.lineTo(6300, 2732)
    outer = outer.lineTo(3629, 2815 + 500)
    
    # 右腹板顶部
    outer = outer.lineTo(3629, 2815)
    
    # 右腹板外边
    # R300 圆心：(3000, 300)
    outer = outer.threePointArc((3000, 300), (3000, 0))
    outer = outer.lineTo(2700, 0)
    
    # 底板闭合
    outer = outer.lineTo(-2700, 0)
    outer = outer.close()
    
    # ========== 内轮廓（右半部分） ==========
    # 底板内顶高程 300mm
    # 底板内半宽 2171mm
    # R300 下承托：圆心 (2471, 300)
    # 腹板内边：从 (2471, 600) 开始，4:1 斜度
    # 顶板内：R50 过渡
    
    inner_right = cq.Workplane("XY").moveTo(0, 300)
    inner_right = inner_right.lineTo(2171, 300)
    
    # R300 下承托
    inner_right = inner_right.threePointArc((2471, 300), (2471, 600))
    
    # 腹板内边（4:1 斜度，水平投影 337.5 = 1350/4）
    inner_right = inner_right.lineTo(2471 - 337.5, 600 + 1350)
    
    # R50 上承托：圆心 (2471-337.5-50, 1950+50) = (2083.5, 2000)
    inner_right = inner_right.threePointArc((2083.5, 2000), (2083.5, 1950))
    
    # 顶板内水平段（到中心）
    inner_right = inner_right.lineTo(900, 1950)
    inner_right = inner_right.lineTo(0, 1950)
    inner_right = inner_right.close()
    
    # ========== 创建实体 ==========
    solid = cq.Workplane("XY").newObject([outer.wires().val()]).extrude(extrude_length)
    
    # 切割右半内腔
    inner_right_solid = cq.Workplane("XY").newObject([inner_right.wires().val()]).extrude(extrude_length + 1)
    solid = solid.cut(inner_right_solid)
    
    # 切割左半内腔（镜像）
    inner_left_wire = cq.Workplane("XY").newObject([inner_right.wires().val()]).mirror((-0.001, 0, 0))
    inner_left_solid = cq.Workplane("XY").newObject([inner_left_wire.wires().val()]).extrude(extrude_length + 1)
    solid = solid.cut(inner_left_solid)
    
    return solid


def create_section_II_II(extrude_length=200):
    """
    创建截面 II-II（支座附近截面）
    
    根据图 3 左侧 1/2 II-II 截面：
    - 全宽：12600（半宽 6300）
    - 梁高：3015
    - 底板厚：487（外侧）+ 432（内侧）
    - 顶板厚：463（外侧）+ 622（内侧）= 1085
    - 腹板位置：距中心线 2700mm
    - 内腔尺寸：根据标注推算
    """
    # ========== 外轮廓 ==========
    outer = cq.Workplane("XY").moveTo(-6300, 0)
    
    # 底板
    outer = outer.lineTo(-2700, 0)
    
    # 左腹板外边（从图 3 推算）
    # 腹板外边从 (2700, 0) 到约 (3329, 2515)
    outer = outer.lineTo(-3329, 2515)
    
    # 左悬臂
    outer = outer.lineTo(-6300, 2732)
    outer = outer.lineTo(-6300, 3015)
    
    # 顶板
    outer = outer.lineTo(6300, 3015)
    
    # 右悬臂
    outer = outer.lineTo(6300, 2732)
    outer = outer.lineTo(3329, 2515)
    
    # 右腹板外边
    outer = outer.lineTo(2700, 0)
    
    # 底板闭合
    outer = outer.lineTo(-2700, 0)
    outer = outer.close()
    
    # ========== 内轮廓（右半部分） ==========
    # 根据图 2 和图 3 推算
    # 底板内顶高程：600mm
    # 内腔半宽：797mm
    # 腹板内侧：从 (947, 600) 垂直向上
    
    inner_right = cq.Workplane("XY").moveTo(0, 600)
    inner_right = inner_right.lineTo(797, 600)
    inner_right = inner_right.lineTo(947, 600)  # 腹板内侧底部
    inner_right = inner_right.lineTo(947, 1930)  # 腹板内侧顶部
    inner_right = inner_right.lineTo(1127.5, 2130)  # 顶板内过渡
    inner_right = inner_right.lineTo(0, 2130)
    inner_right = inner_right.close()
    
    # ========== 创建实体 ==========
    solid = cq.Workplane("XY").newObject([outer.wires().val()]).extrude(extrude_length)
    
    # 切割右半内腔
    inner_right_solid = cq.Workplane("XY").newObject([inner_right.wires().val()]).extrude(extrude_length + 1)
    solid = solid.cut(inner_right_solid)
    
    # 切割左半内腔
    inner_left_wire = cq.Workplane("XY").newObject([inner_right.wires().val()]).mirror((-0.001, 0, 0))
    inner_left_solid = cq.Workplane("XY").newObject([inner_left_wire.wires().val()]).extrude(extrude_length + 1)
    solid = solid.cut(inner_left_solid)
    
    return solid


def create_section_III_III(extrude_length=200):
    """
    创建截面 III-III（支座截面）
    
    根据图 3 右侧 1/2 III-III 截面：
    - 全宽：12600（半宽 6300）
    - 梁高：3032
    - 底板厚：600
    - 顶板中心厚：555 + 530 = 1085
    - 腹板：内侧厚 150，外侧厚 500
    - 内腔半宽：797
    - 内腔顶宽半宽：2255/2 = 1127.5
    """
    # ========== 外轮廓 ==========
    outer = cq.Workplane("XY").moveTo(-6300, 0)
    
    # 底板
    outer = outer.lineTo(-2700, 0)
    
    # 左腹板外边
    outer = outer.lineTo(-3200, 900)  # 腹板外底角
    outer = outer.lineTo(-6171, 3032)  # 腹板外顶
    outer = outer.lineTo(-6300, 3032)
    
    # 顶板
    outer = outer.lineTo(6300, 3032)
    
    # 右腹板外边
    outer = outer.lineTo(3200, 900)
    outer = outer.lineTo(2700, 0)
    
    # 底板闭合
    outer = outer.lineTo(-2700, 0)
    outer = outer.close()
    
    # ========== 内轮廓（右半部分） ==========
    # 底板内顶高程 600mm
    # 内腔半宽 797mm
    # 腹板内侧厚 150mm，从 (947, 600) 到 (947, 1930)
    # 顶板内：从 (947, 1930) 过渡到 (1127.5, 2460)
    
    inner_right = cq.Workplane("XY").moveTo(0, 600)
    inner_right = inner_right.lineTo(797, 600)
    inner_right = inner_right.lineTo(947, 600)  # 腹板内侧底部
    inner_right = inner_right.lineTo(947, 1930)  # 腹板内侧顶部（垂直）
    inner_right = inner_right.lineTo(1127.5, 1930)  # 顶板内过渡
    inner_right = inner_right.lineTo(1127.5, 2460)  # 顶板内缘
    inner_right = inner_right.lineTo(0, 2460)
    inner_right = inner_right.close()
    
    # ========== 创建实体 ==========
    solid = cq.Workplane("XY").newObject([outer.wires().val()]).extrude(extrude_length)
    
    # 切割右半内腔
    inner_right_solid = cq.Workplane("XY").newObject([inner_right.wires().val()]).extrude(extrude_length + 1)
    solid = solid.cut(inner_right_solid)
    
    # 切割左半内腔
    inner_left_wire = cq.Workplane("XY").newObject([inner_right.wires().val()]).mirror((-0.001, 0, 0))
    inner_left_solid = cq.Workplane("XY").newObject([inner_left_wire.wires().val()]).extrude(extrude_length + 1)
    solid = solid.cut(inner_left_solid)
    
    return solid


def create_section_I_I_accurate(extrude_length=200):
    """
    创建截面 I-I（跨中截面）- 精确版
    
    根据图 1 详细尺寸重新计算：
    - 底板高程：0
    - 底板内顶高程：300
    - 腹板外底高程：300（R300 终点）
    - 腹板外顶高程：2815（300+2515）
    - 腹板内底高程：600（300+300）
    - 腹板内顶高程：1950（600+1350）
    - 顶板下表面高程：2500（1950+50+500-50）
    - 顶板上表面高程：2785（2500+285）但实际是 3032
    """
    # 重新分析图 1：
    # 梁底高程：0
    # 顶板上表面高程：3032
    # 底板厚：300
    # 腹板直段高：1350
    # 顶板中心厚：285
    # 所以：底板内顶 300，腹板内顶 300+300+1350=1950
    # 顶板下表面：1950+50(R50)+500 = 2500
    # 顶板上表面：2500+285 = 2785，但图上是 3032
    # 差值：3032-2785 = 247，说明顶板中心加厚了
    
    # 实际上从图 1 看：
    # 顶板中心厚度 = 285 + 500 + 1350 + 300 + 300 - (3032-217) = 需要重新计算
    
    # 从右侧标注：3032 = 217 + 2515 + 300
    # 217 是悬臂端顶板厚
    # 2515 是腹板外边垂直高度
    # 300 是底板厚（含 R300）
    
    # 中心线处：
    # 底板内顶：300
    # 腹板内顶：300 + 300(R300) + 1350 = 1950
    # 顶板内顶：1950 + 50(R50) + 500 = 2500
    # 顶板上表面：2500 + 285 = 2785
    # 但梁高是 3032，所以顶板中心实际厚度 = 3032 - 2500 = 532
    
    # ========== 外轮廓 ==========
    outer = cq.Workplane("XY").moveTo(-6300, 0)
    
    # 左底板（到 R300 起点）
    # R300 圆心在 (-2700-300, 300) = (-3000, 300)
    outer = outer.lineTo(-3000, 0)
    outer = outer.threePointArc((-3000, 300), (-2700, 300))
    
    # 左腹板外边（4:1 斜度）
    # 从 (-2700, 300) 到 (-3629, 2815)
    # 水平：3629-2700 = 929，但图上是 629
    # 重新看：2700 + 629 = 3329，所以腹板外顶 Y = 3329
    # 但图上标注腹板外顶到中心距离是 3629（2700+629）
    
    # 从图 1：腹板外边水平投影 = 629
    # 腹板外底 Y = 2700 + 300 = 3000（R300 终点）
    # 腹板外顶 Y = 3000 + 629 = 3629
    # 腹板外顶 Z = 300 + 2515 = 2815
    
    outer = outer.lineTo(-3629, 2815)
    
    # 腹板顶部垂直段（500mm）
    outer = outer.lineTo(-3629, 2815 + 500)
    
    # 左悬臂根部（顶板下表面）
    # 从图 1：悬臂根部顶板下表面高程 = 2732
    # 2815 + 500 = 3315，但悬臂根部是 2732
    # 说明腹板顶部到悬臂根部有过渡
    
    # 重新看：腹板顶部宽度 900mm（从中心线）
    # 所以腹板内顶 Y = 900
    # 腹板外顶 Y = 900 + 50 + 360 = 1410？不对
    
    # 从图 1 顶部标注：
    # 中心到腹板内边：900
    # 腹板内腔宽：1911
    # 所以腹板内边 Y = 900 + 1911/2 = 900 + 955.5 = 1855.5？不对
    
    # 重新解读图 1：
    # 1911 是内腔半宽（从中心到腹板内边）
    # 900 是顶板内水平段半宽
    
    # 所以：
    # 顶板内边 Y = 900
    # 腹板内边 Y = 900 + 50(R50) = 950
    # 腹板外边 Y = 950 + 360 = 1310
    # 但图上腹板外边 Y = 3629
    
    # 我理解错了。重新看：
    # 1911 标注是从中心到腹板内边的水平距离
    # 所以腹板内边 Y = 1911
    # 腹板外边 Y = 1911 + 360 = 2271
    # 但图上标注 3629
    
    # 再看：2700 + 629 = 3329 是腹板外边底部 Y 坐标
    # 3629 是腹板外边顶部 Y 坐标（因为 4:1 斜度）
    
    # 让我从底部开始重新计算：
    # 底板外边缘 Y = 2700
    # R300 圆心 Y = 2700 + 300 = 3000
    # R300 终点（腹板外底）Y = 3000, Z = 300
    # 腹板外顶 Y = 3000 + 629 = 3629, Z = 300 + 2515 = 2815 ✓
    
    # 腹板内边：
    # 底板内边缘 Y = 2171
    # R300 圆心 Y = 2171 + 300 = 2471
    # R300 终点（腹板内底）Y = 2471, Z = 300 + 300 = 600
    # 腹板内顶 Y = 2471 - 337.5 = 2133.5, Z = 600 + 1350 = 1950
    
    # 顶板内边：
    # R50 圆心 Y = 2133.5 - 50 = 2083.5, Z = 1950 + 50 = 2000
    # R50 终点 Y = 2083.5, Z = 1950
    # 顶板内水平段到 Y = 900
    
    # 但图上标注 1911 是从中心到顶板内垂直边的距离
    # 所以顶板内垂直边 Y = 1911
    # 顶板内水平段 Y = 1911 - 50 - 360 - 50 = 1451？不对
    
    # 看标注：900 是从中心到顶板内垂直边的距离
    # 1911 是从中心到腹板内垂直边的水平距离
    
    # 所以：
    # 顶板内垂直边 Y = 900
    # 顶板内水平段长度 = 1911 - 900 = 1011
    # 但图上内腔顶宽标注是 1911
    
    # 我再看图 1 内腔标注：
    # 2171 是底板内半宽
    # 1911 是顶板内垂直边到中心的距离
    # 900 是顶板内水平段半宽
    
    # 所以内腔形状：
    # 底板内顶：从 (0, 300) 到 (2171, 300)
    # R300：从 (2171, 300) 到 (2471, 600)
    # 腹板内边：从 (2471, 600) 斜到 (2133.5, 1950)
    # R50：从 (2133.5, 1950) 到 (2083.5, 1950)
    # 顶板内斜边：从 (2083.5, 1950) 到 (900, 2450)
    # 顶板内水平段：从 (900, 2450) 到 (0, 2450)
    
    # 但这样顶板内高 = 2450 - 300 = 2150
    # 顶板中心厚 = 3032 - 2450 = 582，不是 285
    
    # 285 是顶板中心厚度，所以顶板下表面高程 = 3032 - 285 = 2747
    # 但腹板内顶高程是 1950，中间有 500+50 的过渡
    # 1950 + 50 + 500 = 2500
    # 2747 - 2500 = 247，这是额外的顶板加厚
    
    # 实际上图 1 中 285 是顶板中心最小厚度
    # 顶板下表面高程 = 2500（腹板内顶 1950 + R50 50 + 斜腹板 500）
    # 顶板上表面高程 = 2500 + 285 = 2785
    # 但梁高是 3032，说明顶板中心有额外加厚 3032 - 2785 = 247
    
    # 看顶部标注：300/217 是顶板端部/根部厚度
    # 中心处顶板厚度应该是变化的
    
    # 我重新解读：285 是顶板中心处厚度
    # 顶板上表面是平的，高程 3032
    # 顶板下表面高程 = 3032 - 285 = 2747
    
    # 但腹板顶部高程是 2815 + 500 = 3315 > 3032，这不对
    
    # 让我重新看腹板顶部标注：
    # 500 是腹板顶部垂直段高度
    # 2815 是腹板外底到腹板外顶的高度
    # 所以腹板外顶高程 = 300 + 2815 = 3115？不对，2815 已经是高程了
    
    # 从右侧标注：3032 = 217 + 2515 + 300
    # 217 是悬臂根部顶板厚
    # 2515 是腹板外边垂直高度
    # 300 是底板 R300 处厚度
    
    # 所以腹板外顶高程 = 300 + 2515 = 2815 ✓
    # 腹板顶部还有 500mm 高的垂直段
    # 腹板顶高程 = 2815 + 500 = 3315
    # 但这超过了梁高 3032，说明 500 不是垂直段高度
    
    # 看内腔标注：500 是顶板内腔高度的一部分
    # 1350 是腹板内边直段高度
    # 300 是 R300 高度
    # 300 是底板厚度
    # 285 是顶板中心厚度
    
    # 总高 = 300 + 300 + 1350 + 500 + 285 = 2735
    # 但梁高是 3032，差 297mm
    
    # 我再看：2515 是腹板外边垂直高度
    # 300 是底板厚
    # 217 是悬臂根部顶板厚
    # 2515 + 300 + 217 = 3032 ✓
    
    # 所以腹板外顶高程 = 2815
    # 顶板下表面在腹板处高程 = 2815
    # 顶板上表面高程 = 2815 + 217 = 3032 ✓
    
    # 中心处：
    # 顶板下表面高程 = 3032 - 285 = 2747
    # 腹板内顶高程 = 1950
    # 过渡段高度 = 2747 - 1950 = 797
    # 其中 R50 占 50，斜段占 747
    
    # 好的，现在清楚了。让我重新建模：
    
    # ========== 外轮廓 ==========
    outer = cq.Workplane("XY").moveTo(-6300, 0)
    
    # 左底板
    outer = outer.lineTo(-3000, 0)
    # 左 R300
    outer = outer.threePointArc((-3000, 300), (-2700, 300))
    # 左腹板外边
    outer = outer.lineTo(-3629, 2815)
    # 左悬臂根部
    outer = outer.lineTo(-6300, 2815)
    outer = outer.lineTo(-6300, 3032)
    # 顶板
    outer = outer.lineTo(6300, 3032)
    # 右悬臂
    outer = outer.lineTo(6300, 2815)
    outer = outer.lineTo(3629, 2815)
    # 右腹板外边
    outer = outer.threePointArc((3000, 300), (3000, 0))
    outer = outer.lineTo(2700, 0)
    # 底板闭合
    outer = outer.lineTo(-2700, 0)
    outer = outer.close()
    
    # ========== 内轮廓（右半部分） ==========
    # 底板内顶高程 300
    # 底板内半宽 2171
    # 腹板内底：R300 终点 (2471, 600)
    # 腹板内顶：(2133.5, 1950)
    # 顶板内：R50 过渡到 (900, 2450)
    # 顶板下表面高程：2747（3032-285）
    
    inner_right = cq.Workplane("XY").moveTo(0, 300)
    inner_right = inner_right.lineTo(2171, 300)
    # R300
    inner_right = inner_right.threePointArc((2471, 300), (2471, 600))
    # 腹板内边
    inner_right = inner_right.lineTo(2133.5, 1950)
    # R50
    inner_right = inner_right.threePointArc((2083.5, 2000), (2083.5, 1950))
    # 顶板内斜段（到顶板下表面）
    # 顶板下表面高程 2747
    # 从 (2083.5, 1950) 斜到 (900, 2747)
    inner_right = inner_right.lineTo(900, 2747)
    inner_right = inner_right.lineTo(0, 2747)
    inner_right = inner_right.close()
    
    # ========== 创建实体 ==========
    solid = cq.Workplane("XY").newObject([outer.wires().val()]).extrude(extrude_length)
    
    # 切割内腔
    inner_right_solid = cq.Workplane("XY").newObject([inner_right.wires().val()]).extrude(extrude_length + 1)
    solid = solid.cut(inner_right_solid)
    
    inner_left_wire = cq.Workplane("XY").newObject([inner_right.wires().val()]).mirror((-0.001, 0, 0))
    inner_left_solid = cq.Workplane("XY").newObject([inner_left_wire.wires().val()]).extrude(extrude_length + 1)
    solid = solid.cut(inner_left_solid)
    
    return solid


def create_all_sections():
    """创建所有三个截面并导出"""
    
    print("创建截面 I-I（跨中截面）...")
    section_I = create_section_I_I_accurate(200)
    cq.exporters.export(section_I, 'section_I_I.step')
    cq.exporters.export(section_I, 'section_I_I.stl')
    print("  已保存：section_I_I.step, section_I_I.stl")
    
    print("创建截面 II-II（支座附近截面）...")
    section_II = create_section_II_II(200)
    cq.exporters.export(section_II, 'section_II_II.step')
    cq.exporters.export(section_II, 'section_II_II.stl')
    print("  已保存：section_II_II.step, section_II_II.stl")
    
    print("创建截面 III-III（支座截面）...")
    section_III = create_section_III_III(200)
    cq.exporters.export(section_III, 'section_III_III.step')
    cq.exporters.export(section_III, 'section_III_III.stl')
    print("  已保存：section_III_III.step, section_III_III.stl")
    
    print("\n所有截面已创建并导出！")
    print("文件格式：STEP (用于 CAD 编辑) 和 STL (用于 3D 查看)")


if __name__ == "__main__":
    create_all_sections()
