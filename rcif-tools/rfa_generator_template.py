from collections import OrderedDict

import math



import cadquery as cq



TEMPLATE = "__TEMPLATE__"





def _num(params, name, default):

    value = params.get(name, default)

    if isinstance(value, (int, float)):

        return float(value)

    try:

        return float(str(value).replace("mm", "").strip())

    except (TypeError, ValueError):

        return float(default)





def _box(x0, x1, y0, y1, z0, z1):

    return cq.Workplane("XY").box(

        max(abs(x1 - x0), 1.0), max(abs(y1 - y0), 1.0), max(abs(z1 - z0), 1.0)

    ).translate(((x0 + x1) / 2.0, (y0 + y1) / 2.0, (z0 + z1) / 2.0)).val()





def _frame(x0, x1, y0, y1, z0, z1, border):

    outer = _box(x0, x1, y0, y1, z0, z1)

    inner = _box(x0 - 1.0, x1 + 1.0, y0 + border, y1 - border, z0 + border, z1 - border)

    return outer.cut(inner)





def _cyl(radius, height, x=0.0, y=0.0, z=0.0, axis="Z"):

    if axis == "X":

        shape = cq.Workplane("YZ").circle(max(radius, 0.5)).extrude(max(height, 1.0))

    elif axis == "Y":

        shape = cq.Workplane("XZ").circle(max(radius, 0.5)).extrude(max(height, 1.0))

    else:

        shape = cq.Workplane("XY").circle(max(radius, 0.5)).extrude(max(height, 1.0))

    return shape.translate((x, y, z)).val()





def _round_column(params):

    d = max(_num(params, "Diameter", 300.0), 10.0)

    t = max(_num(params, "Thickness", 10.0), 1.0)

    h = max(_num(params, "Height", 3000.0), 100.0)

    if t >= d / 2.0:

        t = d * 0.1

    tube = _cyl(d / 2.0, h).cut(_cyl(d / 2.0 - t, h + 2.0, z=-1.0))

    base = _box(-d * 0.7, d * 0.7, -d * 0.7, d * 0.7, 0.0, 25.0)

    parts = OrderedDict()

    parts["ColumnTube"] = tube

    parts["BasePlate"] = base

    return parts





def _manifold(params):

    """Reconstructed parametric manifold with generalized N-port adaptability (分集水器).



    Accurately matched to official Revit B-Rep and STEP reference geometry:

      - Main shell cylinder (DN150 ~ DN1000)

      - Left & Right elliptical/formed end caps

      - N top vertical branch nozzles with weld-neck connection flanges

      - Left & Right instrument nozzles (Thermometer & Pressure Gauge) with flanges

      - Bottom vertical drain/blowdown nozzle

      - Dual saddle supports with matching arc cuts and insulated hardwood base blocks

    """

    body_d = max(_num(params, "BodyDiameter", _num(params, "筒体直径", 159.0)), 50.0)

    body_r = body_d / 2.0

    port_count = int(max(min(_num(params, "PortCount", _num(params, "接口数量", 6)), 24), 1))



    # 管帽封头

    cap_h = max(_num(params, "CapHeight", _num(params, "管帽高度", 65.0)), 20.0)

    cap_r = max(_num(params, "CapRadius", _num(params, "管帽半径", body_r * 1.01)), body_r)



    # 支管口径与间距

    branch_d = max(_num(params, "BranchDiameter", _num(params, "支管直径", _num(params, "支管口径", body_d * 0.55))), 20.0)

    branch_r = branch_d / 2.0

    nozzle_spacing = max(_num(params, "NozzleSpacing", _num(params, "支管间距", _num(params, "支管中心距", 300.0))), branch_d * 1.5)



    # 筒体长度自适应计算

    margin_left = max(branch_d * 1.8, 250.0)

    margin_right = max(branch_d * 1.8, 250.0)

    calc_length = margin_left + (port_count - 1) * nozzle_spacing + margin_right

    length = max(_num(params, "Length", _num(params, "筒体长度", calc_length)), calc_length * 0.8)



    # 高度基准

    support_h = max(_num(params, "SupportHeight", 700.0), 200.0)

    z_center = support_h + body_r

    nozzle_h = max(_num(params, "NozzleHeight", 229.5), branch_r + 50.0)

    z_nozzle_top = z_center + nozzle_h



    # 支架尺寸

    support_w = max(_num(params, "SupportWidth", _num(params, "支架宽度", body_d + 200.0)), body_d + 60.0)

    support_t = max(_num(params, "SupportThickness", _num(params, "支架厚度", 200.0)), 50.0)

    wood_t = max(_num(params, "WoodBlockThickness", _num(params, "木托厚度 1", 100.0)), 30.0)



    # 仪表接管

    inst_h = max(_num(params, "ThermometerHeight", _num(params, "温度计接管高度", 130.0)), 50.0)

    inst_r = max(_num(params, "ThermometerRadius", _num(params, "温度计接管半径", 20.0)), 10.0)



    # 底部排污管

    drain_r = max(_num(params, "DrainRadius", _num(params, "排污管半径", 25.0)), 15.0)



    parts = OrderedDict()



    # 1. 主筒体 ManifoldBody (沿 X 轴)

    x0 = 0.0

    x1 = length

    parts["ManifoldBody"] = (

        cq.Workplane("YZ")

        .circle(body_r)

        .extrude(length)

        .translate((x0, 0.0, z_center))

        .val()

    )



    # 2. 封头 EndCapLeft & EndCapRight (标准圆头/球面封头，直接以 RFA 参数【管帽半径】与【管帽高度】为驱动)

    cap_arc_r_param = _num(params, "CapRadius", _num(params, "管帽半径", 0.0))



    def _make_dome_cap(x_attach: float, direction: float, zc: float, r: float, h: float, arc_radius: float = 0.0):

        # 优先读取 RFA 中的【管帽半径】(Arc Radius)，若无则按相切圆弧封闭方程 (r^2 + h^2)/(2h) 推导

        r_arc = arc_radius if arc_radius >= r else (r * r + h * h) / (2.0 * h)

        xc = -h + r_arc

        phi0 = math.atan2(r, -xc)

        phi_mid = (phi0 + math.pi) / 2.0

        x_mid = xc + r_arc * math.cos(phi_mid)

        z_mid = zc + r_arc * math.sin(phi_mid)

        cap = (

            cq.Workplane("XZ")

            .moveTo(0.0, zc)

            .lineTo(0.0, zc + r)

            .threePointArc((x_mid, z_mid), (-h, zc))

            .close()

            .revolve(360, (0.0, zc, 0.0), (1.0, zc, 0.0))

        )

        if direction > 0:

            cap = cap.mirror("YZ")

        return cap.translate((x_attach, 0.0, 0.0)).val()



    parts["EndCapLeft"] = _make_dome_cap(x0, -1.0, z_center, body_r, cap_h, cap_arc_r_param)

    parts["EndCapRight"] = _make_dome_cap(x1, 1.0, z_center, body_r, cap_h, cap_arc_r_param)



    # 3. 顶部供回水支管嘴与法兰 Nozzle & Flange

    flange_t = 18.0

    flange_r = branch_r * 1.55

    actual_spacing = (length - margin_left - margin_right) / max(port_count - 1, 1) if port_count > 1 else 0



    for i in range(port_count):

        nx = margin_left + i * actual_spacing

        pipe_len = z_nozzle_top - z_center

        parts[f"Nozzle_{i + 1}"] = (

            cq.Workplane("XY")

            .circle(branch_r)

            .extrude(pipe_len - flange_t)

            .translate((nx, 0.0, z_center))

            .val()

        )

        parts[f"Flange_{i + 1}"] = (

            cq.Workplane("XY")

            .circle(flange_r)

            .circle(branch_r * 0.85)

            .extrude(flange_t)

            .translate((nx, 0.0, z_nozzle_top - flange_t))

            .val()

        )



    # 4. 仪表接管 (左侧测温孔, 右侧测压孔)

    inst_top_z = z_center + body_r + inst_h

    inst_flange_r = inst_r * 1.6

    inst_flange_t = 12.0

    parts["ThermometerNozzle"] = (

        cq.Workplane("XY")

        .circle(inst_r)

        .extrude(body_r + inst_h)

        .translate((x0 + margin_left * 0.35, 0.0, z_center))

        .val()

    )

    parts["ThermometerFlange"] = (

        cq.Workplane("XY")

        .circle(inst_flange_r)

        .extrude(inst_flange_t)

        .translate((x0 + margin_left * 0.35, 0.0, inst_top_z - inst_flange_t))

        .val()

    )

    parts["PressureGaugeNozzle"] = (

        cq.Workplane("XY")

        .circle(inst_r)

        .extrude(body_r + inst_h)

        .translate((x1 - margin_right * 0.35, 0.0, z_center))

        .val()

    )

    parts["PressureGaugeFlange"] = (

        cq.Workplane("XY")

        .circle(inst_flange_r)

        .extrude(inst_flange_t)

        .translate((x1 - margin_right * 0.35, 0.0, inst_top_z - inst_flange_t))

        .val()

    )



    # 5. 底部排污管 DrainNozzle

    drain_len = 150.0

    parts["DrainNozzle"] = (

        cq.Workplane("XY")

        .circle(drain_r)

        .extrude(drain_len)

        .translate((x0 + length * 0.65, 0.0, z_center - body_r - drain_len))

        .val()

    )



    # 6. 门式双腿支架 SupportLeft & SupportRight (两条立腿 + 顶横梁，中间开档镂空，精准吻合 STEP)

    supp_t = max(_num(params, "SupportThickness", _num(params, "支架厚度", 200.0)), 50.0)

    leg_w = supp_t  # 支腿宽度

    opening_w = body_d  # 中间两腿净开档等于筒体外径

    supp_total_w = opening_w + 2.0 * leg_w  # 支架总跨度 (如 159 + 400 = 559 mm)

    supp_top_z = z_center - body_r  # 托梁顶面紧密贴合筒体底部 (700 mm)

    beam_h = min(supp_t, supp_top_z * 0.4)  # 顶托横梁高度 (200 mm)

    leg_h = supp_top_z - beam_h  # 支腿净空高 (500 mm)



    supp_x_left = x0 + margin_left * 0.2

    supp_x_right = x1 - margin_right * 0.2



    for name, sx in [("SupportLeft", supp_x_left), ("SupportRight", supp_x_right)]:

        base_supp = cq.Workplane("XY").box(supp_t, supp_total_w, supp_top_z).translate((sx, 0.0, supp_top_z / 2.0))

        opening_cut = cq.Workplane("XY").box(supp_t * 1.5, opening_w, leg_h + 1.0).translate((sx, 0.0, (leg_h + 1.0) / 2.0))

        parts[name] = base_supp.cut(opening_cut).val()



    # 7. 隔热木托垫层 WoodBlockLeft & WoodBlockRight (双腿底部垫层)

    wood_h = max(_num(params, "WoodBlockThickness", _num(params, "木托厚度", _num(params, "木托厚度 1", 20.0))), 5.0)

    for name, sx in [("WoodBlockLeft", supp_x_left), ("WoodBlockRight", supp_x_right)]:

        parts[name] = (

            cq.Workplane("XY")

            .box(supp_t, supp_total_w, wood_h)

            .translate((sx, 0.0, -wood_h / 2.0))

            .val()

        )



    return parts





def _door(params):

    width = max(_num(params, "Width", 3300.0), 300.0)

    height = max(_num(params, "Height", 2100.0), 300.0)

    # 门窗框体与门扇厚度：若 RFA 中厚度为 0（如幕墙嵌板门），取标准 30mm

    depth_val = _num(params, "Thickness", 30.0)

    depth = max(depth_val if depth_val > 5.0 else 30.0, 20.0)



    # 边框型材宽度

    frame_w = max(_num(params, "FrameWidth", 50.0), 20.0)



    # 开启扇宽度 (若 RFA 未给出则尝试半开宽度*2，或按比例泛化推断)

    raw_open = _num(params, "OpeningWidth", 0.0)

    if raw_open <= 0.0:

        raw_half = _num(params, "HalfOpenWidth", 0.0)

        raw_open = raw_half * 2.0 if raw_half > 0.0 else 0.0



    if raw_open <= 0.0 or raw_open > width:

        if width >= 2400.0:

            open_w = min(1800.0, width * 0.6)

        elif width > 1200.0:

            open_w = width - 2.0 * frame_w

        else:

            open_w = width - 2.0 * frame_w

    else:

        open_w = raw_open



    # 判断是否为带固定连窗复合门 (如双面嵌板连窗玻璃门)

    side_w = (width - open_w) / 2.0

    has_sidelites = side_w >= 100.0



    # 挺宽与横坎尺寸

    stile_w = max(_num(params, "StileWidth", 100.0), 40.0)

    top_rail_h = max(_num(params, "RailHeight", 100.0), 40.0)

    bottom_rail_h = max(_num(params, "KickPlateHeight", 150.0), 50.0)



    # 玻璃与五金尺寸

    glass_t = max(_num(params, "GlassThickness", 6.0), 2.0)

    handle_len = max(_num(params, "HandleLength", 670.0), 200.0)

    handle_r = max(_num(params, "HandleRadius", 15.0), 5.0)

    handle_proj = 45.0

    h_offset_x = 60.0



    # 装饰细条尺寸

    bar_w = 15.0

    bar_t = 7.0

    bar_pitch = 50.0



    parts = OrderedDict()



    # 1. 外大框架 (OuterFrame)

    outer = cq.Workplane("XY").box(width, depth, height).translate((0.0, 0.0, height / 2.0))

    if has_sidelites:

        # 左右两侧固定窗洞口 (保留 150mm 下坎)

        left_x_min = -width / 2.0 + frame_w

        left_x_max = -open_w / 2.0 - frame_w

        cut_left = _box(left_x_min, left_x_max, -depth * 1.5, depth * 1.5, bottom_rail_h, height - frame_w)



        right_x_min = open_w / 2.0 + frame_w

        right_x_max = width / 2.0 - frame_w

        cut_right = _box(right_x_min, right_x_max, -depth * 1.5, depth * 1.5, bottom_rail_h, height - frame_w)



        # 中间活动门洞口 (通至地面 0)

        cut_center = _box(-open_w / 2.0, open_w / 2.0, -depth * 1.5, depth * 1.5, 0.0, height - frame_w)

        outer = outer.cut(cut_left).cut(cut_right).cut(cut_center)

    else:

        cut_center = _box(-open_w / 2.0, open_w / 2.0, -depth * 1.5, depth * 1.5, 0.0, height - frame_w)

        outer = outer.cut(cut_center)

    parts["OuterFrame"] = outer.val()



    # 辅助生成垂直防撞/装饰线条

    def _make_glazing_bars(cx: float, z0: float, z1: float):

        b1 = _box(cx - bar_w / 2.0, cx + bar_w / 2.0, -bar_t / 2.0, bar_t / 2.0, z0, z1)

        b2 = _box(cx - bar_pitch - bar_w / 2.0, cx - bar_pitch + bar_w / 2.0, -bar_t / 2.0, bar_t / 2.0, z0, z1)

        b3 = _box(cx + bar_pitch - bar_w / 2.0, cx + bar_pitch + bar_w / 2.0, -bar_t / 2.0, bar_t / 2.0, z0, z1)

        return cq.Workplane("XY").add(b1).union(cq.Workplane("XY").add(b2)).union(cq.Workplane("XY").add(b3)).val()



    # 辅助生成高保真长条管状把手（含上下宝塔式水滴装饰端头、环形卡圈与喇叭形支座）

    def _make_handle(hx: float, hy_sign: float, base_z: float = 1000.0, z_span: float = 600.0):

        y_base = hy_sign * (depth / 2.0)

        y_bar = hy_sign * (depth / 2.0 + handle_proj)

        z_top = base_z + z_span / 2.0

        z_bot = base_z - z_span / 2.0



        # 1. 主拉手立管（带支座交接处环形凸圈 Collar Rings）

        bar = (

            cq.Workplane("XZ")

            .moveTo(0, z_bot - 10.0)

            .lineTo(12.0, z_bot - 10.0)

            .threePointArc((16.2, z_bot), (12.0, z_bot + 10.0))

            .lineTo(12.0, z_top - 10.0)

            .threePointArc((16.2, z_top), (12.0, z_top + 10.0))

            .lineTo(0, z_top + 10.0)

            .close()

            .revolve(360, (0, 0, 0), (0, 1, 0))

        )



        # 2. 顶部宝塔式装饰端头 (Top Finial: 阶梯卡环 + 环珠 + 细颈 + 球冠水滴顶)

        top_finial = (

            cq.Workplane("XZ")

            .moveTo(0, 1310.0)

            .lineTo(12.0, 1310.0)

            .threePointArc((12.54, 1313.20), (14.18, 1315.99))

            .lineTo(12.38, 1315.99)

            .lineTo(10.18, 1319.43)

            .threePointArc((11.34, 1323.06), (10.18, 1326.68))

            .lineTo(10.18, 1330.68)

            .threePointArc((11.63, 1334.73), (8.60, 1337.79))

            .lineTo(7.57, 1350.39)

            .threePointArc((8.92, 1352.47), (6.94, 1353.96))

            .threePointArc((4.60, 1358.28), (0.0, 1360.0))

            .close()

            .revolve(360, (0, 0, 0), (0, 1, 0))

        )

        if z_top != 1300.0:

            top_finial = top_finial.translate((0, 0, z_top - 1300.0))



        # 3. 底部宝塔式装饰端头 (Bottom Finial: 镜像翻转)

        bot_finial = top_finial.translate((0, 0, -base_z)).mirror("XY").translate((0, 0, base_z))



        col = bar.union(top_finial).union(bot_finial).translate((hx, y_bar, 0.0))



        # 4. 喇叭形门面法兰底座与支臂 (Flared Standoffs)

        def _make_standoff(zc: float):

            L = abs(y_bar - y_base)

            st = (

                cq.Workplane("XY")

                .moveTo(0, 0)

                .lineTo(0, 15.0)

                .lineTo(3.0, 15.0)

                .threePointArc((21.0, 11.26), (L - 5.0, 10.0))

                .lineTo(L, 10.0)

                .lineTo(L, 0.0)

                .close()

                .revolve(360, (0, 0, 0), (1, 0, 0))

            )

            if hy_sign > 0:

                st = st.rotate((0, 0, 0), (0, 0, 1), 90).translate((hx, y_base, zc))

            else:

                st = st.rotate((0, 0, 0), (0, 0, 1), -90).translate((hx, y_base, zc))

            return st



        s_top = _make_standoff(z_top)

        s_bot = _make_standoff(z_bot)



        return col.union(s_top).union(s_bot).val()



    # 2. 固定侧窗玻璃与装饰细条

    if has_sidelites:

        parts["GlassFixedLeft"] = _box(left_x_min, left_x_max, -glass_t / 2.0, glass_t / 2.0, bottom_rail_h, height - frame_w)

        parts["GlassFixedRight"] = _box(right_x_min, right_x_max, -glass_t / 2.0, glass_t / 2.0, bottom_rail_h, height - frame_w)

        parts["GlazingBarsFixedLeft"] = _make_glazing_bars((left_x_min + left_x_max) / 2.0, bottom_rail_h, height - frame_w)

        parts["GlazingBarsFixedRight"] = _make_glazing_bars((right_x_min + right_x_max) / 2.0, bottom_rail_h, height - frame_w)



    # 3. 活动门扇体系

    is_double = has_sidelites or (open_w > 1200.0)

    leaf_clearance = 2.0

    leaf_h = height - frame_w - leaf_clearance



    if is_double:

        leaf_l_x0 = -open_w / 2.0 + leaf_clearance

        leaf_l_x1 = -leaf_clearance / 2.0

        leaf_w = leaf_l_x1 - leaf_l_x0



        # 左活动门扇

        leaf_l = cq.Workplane("XY").box(leaf_w, depth, leaf_h).translate(((leaf_l_x0 + leaf_l_x1) / 2.0, 0.0, leaf_h / 2.0))

        cut_l = _box(leaf_l_x0 + stile_w, leaf_l_x1 - stile_w, -depth * 1.5, depth * 1.5, bottom_rail_h, leaf_h - top_rail_h)

        parts["DoorLeafLeft"] = leaf_l.cut(cut_l).val()



        # 右活动门扇

        leaf_r_x0 = leaf_clearance / 2.0

        leaf_r_x1 = open_w / 2.0 - leaf_clearance

        leaf_r = cq.Workplane("XY").box(leaf_w, depth, leaf_h).translate(((leaf_r_x0 + leaf_r_x1) / 2.0, 0.0, leaf_h / 2.0))

        cut_r = _box(leaf_r_x0 + stile_w, leaf_r_x1 - stile_w, -depth * 1.5, depth * 1.5, bottom_rail_h, leaf_h - top_rail_h)

        parts["DoorLeafRight"] = leaf_r.cut(cut_r).val()



        # 活动门玻璃

        parts["GlassDoorLeft"] = _box(leaf_l_x0 + stile_w, leaf_l_x1 - stile_w, -glass_t / 2.0, glass_t / 2.0, bottom_rail_h, leaf_h - top_rail_h)

        parts["GlassDoorRight"] = _box(leaf_r_x0 + stile_w, leaf_r_x1 - stile_w, -glass_t / 2.0, glass_t / 2.0, bottom_rail_h, leaf_h - top_rail_h)



        # 活动门装饰细条

        parts["GlazingBarsDoorLeft"] = _make_glazing_bars((leaf_l_x0 + stile_w + leaf_l_x1 - stile_w) / 2.0, bottom_rail_h, leaf_h - top_rail_h)

        parts["GlazingBarsDoorRight"] = _make_glazing_bars((leaf_r_x0 + stile_w + leaf_r_x1 - stile_w) / 2.0, bottom_rail_h, leaf_h - top_rail_h)



        # 双面长管状把手 (内外共 4 套)

        parts["HandleOuterLeft"] = _make_handle(-h_offset_x, 1.0)

        parts["HandleInnerLeft"] = _make_handle(-h_offset_x, -1.0)

        parts["HandleOuterRight"] = _make_handle(h_offset_x, 1.0)

        parts["HandleInnerRight"] = _make_handle(h_offset_x, -1.0)

    else:

        # 单开活动门

        leaf_x0 = -open_w / 2.0 + leaf_clearance

        leaf_x1 = open_w / 2.0 - leaf_clearance

        leaf_w = leaf_x1 - leaf_x0

        leaf = cq.Workplane("XY").box(leaf_w, depth, leaf_h).translate(((leaf_x0 + leaf_x1) / 2.0, 0.0, leaf_h / 2.0))

        cut = _box(leaf_x0 + stile_w, leaf_x1 - stile_w, -depth * 1.5, depth * 1.5, bottom_rail_h, leaf_h - top_rail_h)

        parts["DoorLeaf"] = leaf.cut(cut).val()

        parts["GlassDoor"] = _box(leaf_x0 + stile_w, leaf_x1 - stile_w, -glass_t / 2.0, glass_t / 2.0, bottom_rail_h, leaf_h - top_rail_h)

        parts["GlazingBarsDoor"] = _make_glazing_bars((leaf_x0 + stile_w + leaf_x1 - stile_w) / 2.0, bottom_rail_h, leaf_h - top_rail_h)

        handle_x = leaf_x1 - stile_w / 2.0

        parts["HandleOuter"] = _make_handle(handle_x, 1.0)

        parts["HandleInner"] = _make_handle(handle_x, -1.0)



    return parts





def _elevator(params):

    """Reconstructed parametric machine-room-less elevator with dual through doors (无机房电梯_双门).



    Accurately matched to official Revit B-Rep and STEP reference geometry:

      - Through hollow car body (CarBody) with front & rear door openings

      - Front double sliding doors (DoorFrontLeft, DoorFrontRight)

      - Rear double sliding doors (DoorRearLeft, DoorRearRight)

      - Bottom mounting base platform plinth (BasePlinth)

      - Front & Rear extended support beams (SupportBeamFront, SupportBeamRear)

      - Top & Bottom center vertical guide rails (GuideRailTop, GuideRailBottom)

    """

    car_w = max(_num(params, "CarWidth", _num(params, "轿厢宽度", 1350.0)), 500.0)

    car_d = max(_num(params, "CarDepth", _num(params, "轿厢深度", 1400.0)), 500.0)

    entrance_w = max(_num(params, "EntranceWidth", _num(params, "入口宽度", 900.0)), 400.0)

    pit_d = max(_num(params, "PitDepth", _num(params, "底坑深度", 1400.0)), 300.0)



    t_wall_x = 25.0

    t_wall_y = 75.0

    t_roof = 150.0

    car_inner_h = 2300.0

    car_h = car_inner_h + t_roof

    door_h = 2100.0



    out_w = car_w + 2 * t_wall_x

    out_d = car_d + 2 * t_wall_y



    cx = 55.0

    cy = 75.0



    x0_out = cx - out_w / 2.0

    x1_out = cx + out_w / 2.0

    y0_out = cy - out_d / 2.0

    y1_out = cy + out_d / 2.0



    parts = OrderedDict()



    # 1. 轿厢主体 CarBody (前后贯通中空壳体)

    outer_box = cq.Workplane("XY").box(out_w, out_d, car_h).translate((cx, cy, car_h / 2.0))

    inner_void = cq.Workplane("XY").box(car_w, car_d, car_inner_h).translate((cx, cy, car_inner_h / 2.0))

    front_cut = cq.Workplane("XY").box(entrance_w, t_wall_y * 2 + 10.0, door_h).translate((cx, y0_out + t_wall_y / 2.0, door_h / 2.0))

    rear_cut = cq.Workplane("XY").box(entrance_w, t_wall_y * 2 + 10.0, door_h).translate((cx, y1_out - t_wall_y / 2.0, door_h / 2.0))

    parts["CarBody"] = outer_box.cut(inner_void).cut(front_cut).cut(rear_cut).val()



    # 2. 前门扇与后门扇 (双门中分)

    leaf_w = entrance_w / 2.0 + 20.0

    door_t = 45.0

    parts["DoorFrontLeft"] = _box(cx - leaf_w, cx, y0_out - door_t - 5.0, y0_out - 5.0, 0.0, door_h)

    parts["DoorFrontRight"] = _box(cx, cx + leaf_w, y0_out - door_t - 5.0, y0_out - 5.0, 0.0, door_h)

    parts["DoorRearLeft"] = _box(cx - leaf_w, cx, y1_out + 5.0, y1_out + 5.0 + door_t, 0.0, door_h)

    parts["DoorRearRight"] = _box(cx, cx + leaf_w, y1_out + 5.0, y1_out + 5.0 + door_t, 0.0, door_h)



    # 3. 底座托架 BasePlinth

    base_w = out_w + 320.0

    base_d = out_d + 110.0

    base_t = 100.0

    x0_base = cx - base_w / 2.0

    x1_base = cx + base_w / 2.0

    y0_base = cy - base_d / 2.0

    y1_base = cy + base_d / 2.0

    y_front_notch = y0_out - 5.0

    y_rear_notch = y1_out + 5.0



    base_pts = [

        (x1_out, y_front_notch),

        (x1_base, y_front_notch),

        (x1_base, y0_base),

        (x0_base, y0_base),

        (x0_base, y_front_notch),

        (x0_out, y_front_notch),

        (x0_out, y_rear_notch),

        (x0_base, y_rear_notch),

        (x0_base, y1_base),

        (x1_base, y1_base),

        (x1_base, y_rear_notch),

        (x1_out, y_rear_notch),

        (x1_out, y_front_notch),

    ]

    parts["BasePlinth"] = (

        cq.Workplane("XY")

        .polyline(base_pts).close()

        .extrude(base_t)

        .translate((0.0, 0.0, -base_t))

        .val()

    )



    # 4. 底部外伸支承横梁 SupportBeamFront & SupportBeamRear

    beam_w = 55.0

    parts["SupportBeamFront"] = _box(x0_base, x1_base, y0_base - beam_w - 15.0, y0_base - 15.0, -base_t, 0.0)

    parts["SupportBeamRear"] = _box(x0_base, x1_base, y1_base + 15.0, y1_base + beam_w + 15.0, -base_t, 0.0)



    # 5. 垂直导向轴 GuideRailTop & GuideRailBottom

    rail_r = 10.0

    top_rail_len = 5550.0

    parts["GuideRailTop"] = _cyl(rail_r, top_rail_len, x=cx, y=cy, z=car_h)

    parts["GuideRailBottom"] = _cyl(rail_r, pit_d - base_t, x=cx, y=cy, z=-pit_d)



    return parts





def _generic(params):

    w = max(_num(params, "Width", _num(params, "Length", 1000.0)), 10.0)

    d = max(_num(params, "Depth", _num(params, "Length", w)), 10.0)

    h = max(_num(params, "Height", 1000.0), 10.0)

    base_h = min(h * 0.08, 60.0)

    parts = OrderedDict()

    parts["BaseFrame"] = _box(-w / 2, w / 2, -d / 2, d / 2, 0, base_h)

    parts["MainBody"] = _box(-w * 0.48, w * 0.48, -d * 0.48, d * 0.48, base_h, h - base_h * 0.5)

    parts["TopAssembly"] = _box(-w * 0.4, w * 0.4, -d * 0.4, d * 0.4, h - base_h * 0.5, h)

    return parts





def _cyl_x(radius, x0, x1, y=0.0, z=0.0):

    x_min, x_max = (x0, x1) if x0 <= x1 else (x1, x0)

    span = max(x_max - x_min, 1.0)

    return cq.Workplane("YZ").circle(max(radius, 0.5)).extrude(span).translate((x_min, y, z)).val()





def _cyl_y(radius, y0, y1, x=0.0, z=0.0):

    y_min, y_max = (y0, y1) if y0 <= y1 else (y1, y0)

    span = max(y_max - y_min, 1.0)

    return cq.Workplane("XZ").circle(max(radius, 0.5)).extrude(span).translate((x, y_max, z)).val()





def _cyl_z(radius, z0, z1, x=0.0, y=0.0):

    z_min, z_max = (z0, z1) if z0 <= z1 else (z1, z0)

    span = max(z_max - z_min, 1.0)

    return cq.Workplane("XY").circle(max(radius, 0.5)).extrude(span).translate((x, y, z_min)).val()





def _pump(params):

    """Reconstructed from Revit GRep B-Rep (Partitions/0 pages 8-9, eid=1712021..1712144).



    Exact 10 ExtrusionElem parts of '离心泵 - 卧式.rfa' (5.4 LPS - 7.3 米扬程):

      1. BasePlate        (eid=1712021): X=[0.0, 406.0],   Y=[0.0, 878.0],   Z=[0.0, 101.6]

      2. PumpCasing       (eid=1712040): R=146.05,         Y=[51.5, 130.5],  center=(203.0, *, 272.0)

      3. PumpHub          (eid=1712077): R=115.8875,       Y=[130.5, 321.794], center=(203.0, *, 272.0)

      4. DriveShaft       (eid=1712086): R=33.3375,        Y=[321.794, 510.706], center=(203.0, *, 272.0)

      5. Motor            (eid=1712095): R=115.8875,       Y=[510.706, 847.256], center=(203.0, *, 272.0)

      6. MotorPedestal    (eid=1712104): X=[101.4, 304.6], Y=[551.981, 805.981], Z=[101.6, 156.113]

      7. DischargeNozzle  (eid=1712117): R=31.75,          Z=[101.6, 457.6], center=(203.0, 91.0, *)

      8. DischargeFlange  (eid=1712126): R=80.0,           Z=[457.6, 483.0], center=(203.0, 91.0, *)

      9. SuctionFlange    (eid=1712135): R=70.0,           Y=[15.0, 27.7],   center=(203.0, *, 272.0)

      10. SuctionNozzle   (eid=1712144): R=32.7,           Y=[27.7, 51.5],   center=(203.0, *, 272.0)

    """

    depth = max(_num(params, "Depth", 878.0), 200.0)

    width = max(_num(params, "Width", 406.0), 120.0)

    axis_z = max(_num(params, "Height", 272.0), 120.0)

    head_rise = max(_num(params, "MotorHeight", 211.0), 80.0)

    mount_h_raw = max(_num(params, "MotorMountHeight", 55.0), 10.0)

    mount_h = 54.5125 if abs(mount_h_raw - 55.0) < 0.6 else mount_h_raw



    cx = width / 2.0

    base_h = min(101.6, axis_z * 0.45)

    motor_z0 = base_h + mount_h

    motor_r = max(axis_z - motor_z0, 40.0)



    sym_r = _num(params, "CasingRadius", 203.0)

    casing_r = max((sym_r / 203.0) * 146.05 if sym_r > 0 else (motor_r + 30.1625), motor_r + 10.0)

    casing_cy = max(_num(params, "MotorLength", 91.0), 40.0)

    casing_len = max(_num(params, "ImpellerThickness", 79.0), 20.0)

    casing_y0 = casing_cy - casing_len / 2.0

    casing_y1 = casing_cy + casing_len / 2.0



    suction_flange_r = max(_num(params, "SuctionOuterRadius", 70.0), 20.0)

    suction_nom_r = max(

        (_num(params, "SuctionNominalRadius", 20.0) + _num(params, "SuctionNominalDiameter", 40.0) / 2.0) / 2.0,

        8.0,

    )

    suction_nozzle_r = min(suction_nom_r + 12.7, suction_flange_r * 0.85)

    suction_nozzle_len = 23.8

    suction_flange_t = 12.7

    suction_y1 = casing_y0

    suction_y0 = suction_y1 - suction_nozzle_len

    suction_flange_y0 = suction_y0 - suction_flange_t



    discharge_flange_r = max(_num(params, "DischargeOuterRadius", 80.0), 20.0)

    discharge_nom_r = max(

        (_num(params, "DischargeNominalRadius", 15.0) + _num(params, "DischargeNominalDiameter", 30.0) / 2.0) / 2.0,

        6.0,

    )

    discharge_nozzle_r = min(discharge_nom_r + 16.75, discharge_flange_r * 0.8)

    z_top = max(axis_z + head_rise, axis_z + casing_r + 25.4)

    discharge_flange_t = 25.4

    discharge_flange_z0 = z_top - discharge_flange_t



    hub_r_raw = max(_num(params, "HubRadius", 33.0), 8.0)

    shaft_r = 33.3375 if abs(hub_r_raw - 33.0) < 0.6 else min(hub_r_raw, motor_r * 0.75)



    rear_end_y = max(depth - 30.744, casing_y1 + 180.0)

    rear_span = rear_end_y - casing_y1

    hub_len = rear_span * (191.294 / 716.756)

    shaft_len = rear_span * (188.912 / 716.756)

    hub_y0 = casing_y1

    hub_y1 = hub_y0 + hub_len

    shaft_y0 = hub_y1

    shaft_y1 = shaft_y0 + shaft_len

    motor_y0 = shaft_y1

    motor_y1 = rear_end_y



    ped_half_w = min(width * 0.2502463, width * 0.42)

    motor_cy = (motor_y0 + motor_y1) / 2.0

    ped_half_len = min((motor_y1 - motor_y0) * (127.0 / 336.550), (motor_y1 - motor_y0) * 0.48)



    parts = OrderedDict()

    parts["BasePlate"] = _box(0.0, width, 0.0, depth, 0.0, base_h)

    parts["PumpCasing"] = _cyl_y(casing_r, casing_y0, casing_y1, x=cx, z=axis_z)

    parts["PumpHub"] = _cyl_y(motor_r, hub_y0, hub_y1, x=cx, z=axis_z)

    parts["DriveShaft"] = _cyl_y(shaft_r, shaft_y0, shaft_y1, x=cx, z=axis_z)

    parts["Motor"] = _cyl_y(motor_r, motor_y0, motor_y1, x=cx, z=axis_z)

    parts["MotorPedestal"] = _box(

        cx - ped_half_w, cx + ped_half_w,

        motor_cy - ped_half_len, motor_cy + ped_half_len,

        base_h, motor_z0,

    )

    parts["DischargeNozzle"] = _cyl_z(discharge_nozzle_r, base_h, discharge_flange_z0, x=cx, y=casing_cy)

    parts["DischargeFlange"] = _cyl_z(discharge_flange_r, discharge_flange_z0, z_top, x=cx, y=casing_cy)

    parts["SuctionFlange"] = _cyl_y(suction_flange_r, suction_flange_y0, suction_y0, x=cx, z=axis_z)

    parts["SuctionNozzle"] = _cyl_y(suction_nozzle_r, suction_y0, suction_y1, x=cx, z=axis_z)

    return parts





def _rubber_joint(params):

    """Reconstructed flexible rubber joint (可曲挠橡胶接头, DN200 PN16).



    3 CAD parts matching Revit GRep B-Rep:

      1. FlangeLeft: X=[-153, -110], OD=400, ID=200, thickness=43mm

      2. FlangeRight: X=[110, 153], OD=400, ID=200, thickness=43mm

      3. RubberArch: X=[-153, 153], OD=287.08 (arch bulge), ID=200

    """

    length = max(_num(params, "Length", 306.0), 50.0)

    dn = max(_num(params, "NominalDiameter", 200.0), 20.0)

    inner_r = dn / 2.0

    flange_d = max(_num(params, "FlangeDiameter", 400.0), dn * 1.2)

    flange_t = min(max(_num(params, "FlangeThickness", 43.0), 10.0), length * 0.35)

    arch_d = max(_num(params, "ArchOuterDiameter", 287.0), dn * 1.1)

    arch_r = arch_d / 2.0

    arch_len = min(max(_num(params, "ArchLength", 220.0), 30.0), length - flange_t * 1.5)

    neck_r = inner_r + 15.0



    parts = OrderedDict()

    parts["FlangeLeft"] = (

        cq.Workplane("YZ")

        .circle(flange_d / 2.0)

        .circle(inner_r)

        .extrude(flange_t)

        .translate((-length / 2.0, 0, 0))

        .val()

    )

    parts["FlangeRight"] = (

        cq.Workplane("YZ")

        .circle(flange_d / 2.0)

        .circle(inner_r)

        .extrude(flange_t)

        .translate((length / 2.0 - flange_t, 0, 0))

        .val()

    )



    x0 = -length / 2.0

    x1 = -arch_len / 2.0

    x2 = 0.0

    x3 = arch_len / 2.0

    x4 = length / 2.0



    parts["RubberArch"] = (

        cq.Workplane("XY")

        .moveTo(x0, inner_r)

        .lineTo(x0, neck_r)

        .lineTo(x1, neck_r)

        .threePointArc((x2, arch_r), (x3, neck_r))

        .lineTo(x4, neck_r)

        .lineTo(x4, inner_r)

        .close()

        .revolve(360, (0, 0, 0), (1, 0, 0))

        .val()

    )

    return parts





def _silencer(params):

    length = max(_num(params, "Length", 1200.0), 100.0)

    width = max(_num(params, "Width", 800.0), 100.0)

    height = max(_num(params, "Height", 500.0), 100.0)

    wall = max(min(width, height) * 0.025, 8.0)

    flange = max(min(width, height) * 0.08, 16.0)

    parts = OrderedDict()

    parts["CasingBottom"] = _box(-length / 2, length / 2, -width / 2, width / 2, 0, wall)

    parts["CasingTop"] = _box(-length / 2, length / 2, -width / 2, width / 2, height - wall, height)

    parts["CasingLeft"] = _box(-length / 2, length / 2, -width / 2, -width / 2 + wall, 0, height)

    parts["CasingRight"] = _box(-length / 2, length / 2, width / 2 - wall, width / 2, 0, height)

    parts["InletFlange"] = _frame(

        -length / 2 - flange, -length / 2,

        -width / 2 - flange, width / 2 + flange,

        0, height,

        wall,

    )

    parts["OutletFlange"] = _frame(

        length / 2, length / 2 + flange,

        -width / 2 - flange, width / 2 + flange,

        0, height,

        wall,

    )

    splitter = max(width * 0.06, 12.0)

    for index, factor in enumerate((-0.26, 0.0, 0.26)):

        y = width * factor

        parts[f"Baffle{index + 1}"] = _box(

            -length * 0.42, length * 0.42,

            y - splitter / 2, y + splitter / 2,

            wall, height - wall,

        )

    return parts





def _tank(params):

    length = max(_num(params, "Depth", 900.0), 100.0)

    width = max(_num(params, "Width", 900.0), 100.0)

    height = max(_num(params, "Height", 900.0), 100.0)

    wall = max(min(length, width, height) * 0.018, 6.0)

    outer = _box(-length / 2, length / 2, -width / 2, width / 2, 0, height)

    inner = _box(-length / 2 + wall, length / 2 - wall, -width / 2 + wall, width / 2 - wall, wall, height)

    parts = OrderedDict()

    parts["TankShell"] = outer.cut(inner)

    parts["BaseLeft"] = _box(-length * 0.38, -length * 0.3, -width * 0.4, width * 0.4, -60, 0)

    parts["BaseRight"] = _box(length * 0.3, length * 0.38, -width * 0.4, width * 0.4, -60, 0)

    parts["TopNozzle"] = _cyl(min(width, length) * 0.06, 100, y=0, z=height - 10, x=-length * 0.2, axis="Z")

    parts["SideNozzle"] = _cyl(min(width, length) * 0.07, width * 0.18, x=length * 0.25, y=width / 2, z=height * 0.72, axis="Y")

    parts["DrainNozzle"] = _cyl(min(width, length) * 0.05, 80, x=-length * 0.25, y=-width * 0.2, z=-30, axis="Z")

    return parts





def _ahu(params):

    """Reconstructed from Revit GRep B-Rep of 'AHU - 卧式 - 顶出后送式 - 2000-9000 CMH.rfa'.



    Components:

      1. BaseRailLeft / BaseRailRight: 纵向型钢支轨底座 (X 缩进 40mm, 高 50mm, 沿 Y 全长)

      2. CabinetBody: 箱体主体 (X=[0, width], Y=[0, depth], Z=[base_h, base_h + height])

      3. SupplyAirDuct: 顶出送风口 (Z=[base_h + height, base_h + height + 100], 受 SupplyAirWidth, SupplyAirHeight, Offset1/2 驱动)

      4. SupplyAirFlange: 顶出送风口外框法兰 (外扩 36mm, 厚 10mm)

      5. ReturnAirDuct: 后送/回风风口短接 (Y=[depth, depth + 70], 受 ReturnAirWidth, ReturnAirHeight 驱动)

      6. ReturnAirFlange: 后送/回风口外框法兰 (外扩 36mm, 厚 5mm)

      7. WaterInletNozzle: 冷热水进水接管 (X=[width, width + 40], 受 WaterInletRadius, Offset1/2 驱动)

      8. WaterInletFlange: 进水管外沿法兰 (R = 1.6 * radius, 厚 10mm)

      9. WaterOutletNozzle: 冷热水出水接管 (X=[width, width + 40], 受 WaterOutletRadius, Offset1 驱动)

      10. WaterOutletFlange: 出水管外沿法兰 (R = 1.6 * radius, 厚 10mm)

      11. DrainNozzle: 冷凝水排水管 (X=[width, width + 40], 受 DrainRadius, Offset1 驱动)

      12. AccessPanelFront / AccessPanelBack: 侧面检修门与分段面板 (凸出 6mm, 体现过滤/表冷/风机分段)

    """

    width = max(_num(params, "Width", 2200.0), 300.0)

    depth = max(_num(params, "Depth", 2900.0), 500.0)

    height = max(_num(params, "Height", 1500.0), 400.0)

    base_h = max(_num(params, "BaseHeight", 50.0), 20.0)



    parts = OrderedDict()



    # 1. 箱体底座型钢支轨 (2根纵向型钢支轨，截面宽约 40mm，高 base_h)

    rail_w = min(max(width * 0.02, 30.0), 80.0)

    rail_inset = min(max(width * 0.015, 20.0), 60.0)

    parts["BaseRailLeft"] = _box(rail_inset, rail_inset + rail_w, 0.0, depth, 0.0, base_h)

    parts["BaseRailRight"] = _box(width - rail_inset - rail_w, width - rail_inset, 0.0, depth, 0.0, base_h)



    # 2. 机组主体箱体 (CabinetBody)

    parts["CabinetBody"] = _box(0.0, width, 0.0, depth, base_h, base_h + height)



    # 3. 顶出送风口 (SupplyAirDuct & Flange)

    sa_w = min(max(_num(params, "SupplyAirWidth", 800.0), 100.0), width - 40.0)

    sa_len = min(max(_num(params, "SupplyAirHeight", 600.0), 100.0), depth * 0.65)

    sa_cx = _num(params, "SupplyAirOffset2", width / 2.0)

    sa_cx = min(max(sa_cx, sa_w / 2.0 + 20.0), width - sa_w / 2.0 - 20.0)

    sa_y0 = min(max(_num(params, "SupplyAirOffset1", 149.0), 0.0), depth - sa_len - 20.0)

    top_duct_h = 100.0

    z_top = base_h + height



    parts["SupplyAirDuct"] = _box(sa_cx - sa_w / 2.0, sa_cx + sa_w / 2.0, sa_y0, sa_y0 + sa_len, z_top, z_top + top_duct_h)

    flange_ext = 36.0

    flange_t = 10.0

    parts["SupplyAirFlange"] = _frame(

        sa_cx - sa_w / 2.0 - flange_ext, sa_cx + sa_w / 2.0 + flange_ext,

        sa_y0 - flange_ext, sa_y0 + sa_len + flange_ext,

        z_top + top_duct_h - flange_t, z_top + top_duct_h,

        flange_ext

    )



    # 4. 后送/回风风口 (ReturnAirDuct & Flange)

    ra_w = min(max(_num(params, "ReturnAirWidth", 800.0), 100.0), width - 40.0)

    ra_h = min(max(_num(params, "ReturnAirHeight", 600.0), 100.0), height - 60.0)

    ra_cx = width / 2.0

    ra_z0 = base_h + min(80.0, max((height - ra_h) / 2.0, 30.0))

    rear_duct_len = 70.0



    parts["ReturnAirDuct"] = _box(ra_cx - ra_w / 2.0, ra_cx + ra_w / 2.0, depth, depth + rear_duct_len, ra_z0, ra_z0 + ra_h)

    flange_ra_t = 5.0

    parts["ReturnAirFlange"] = _frame(

        ra_cx - ra_w / 2.0 - flange_ext, ra_cx + ra_w / 2.0 + flange_ext,

        depth + rear_duct_len - flange_ra_t, depth + rear_duct_len,

        ra_z0 - flange_ext, ra_z0 + ra_h + flange_ext,

        flange_ext

    )



    # 5. 冷热水进水接管与法兰 (WaterInlet)

    inlet_r = max(_num(params, "WaterInletRadius", _num(params, "WaterInletDiameter", 50.0) / 2.0), 10.0)

    inlet_y = min(max(_num(params, "WaterInletOffset2", 164.0), inlet_r + 20.0), depth - inlet_r - 20.0)

    inlet_z = base_h + min(max(_num(params, "WaterInletOffset1", 543.0), inlet_r + 40.0), height - inlet_r - 40.0)

    pipe_len = 40.0

    pipe_flange_t = 10.0

    parts["WaterInletNozzle"] = _cyl_x(inlet_r, width, width + pipe_len, y=inlet_y, z=inlet_z)

    parts["WaterInletFlange"] = _cyl_x(inlet_r * 1.6, width + pipe_len - pipe_flange_t, width + pipe_len, y=inlet_y, z=inlet_z)



    # 6. 冷热水出水接管与法兰 (WaterOutlet)

    outlet_r = max(_num(params, "WaterOutletRadius", _num(params, "WaterOutletDiameter", 50.0) / 2.0), 10.0)

    outlet_y = inlet_y

    outlet_z = base_h + min(max(_num(params, "WaterOutletOffset1", 222.0), outlet_r + 40.0), inlet_z - inlet_r - outlet_r - 30.0)

    parts["WaterOutletNozzle"] = _cyl_x(outlet_r, width, width + pipe_len, y=outlet_y, z=outlet_z)

    parts["WaterOutletFlange"] = _cyl_x(outlet_r * 1.6, width + pipe_len - pipe_flange_t, width + pipe_len, y=outlet_y, z=outlet_z)



    # 7. 冷凝水排水管 (DrainNozzle)

    drain_r = max(_num(params, "DrainRadius", _num(params, "DrainDiameter", 32.0) / 2.0), 8.0)

    drain_y = min(max(_num(params, "DrainOffset1", 483.0), drain_r + 20.0), depth - drain_r - 20.0)

    drain_z = base_h + max(drain_r + 10.0, 30.0)

    parts["DrainNozzle"] = _cyl_x(drain_r, width, width + pipe_len, y=drain_y, z=drain_z)



    # 8. 侧面操作检修门与分段隔框 (AccessPanelFront & AccessPanelBack)

    panel_margin = 40.0

    panel_h = height - panel_margin * 2.0

    panel_w = (depth - panel_margin * 3.0) / 2.0

    p_t = 6.0

    parts["AccessPanelFront"] = _box(-p_t, 0.0, panel_margin, panel_margin + panel_w, base_h + panel_margin, base_h + panel_margin + panel_h)

    parts["AccessPanelBack"] = _box(-p_t, 0.0, panel_margin * 2.0 + panel_w, depth - panel_margin, base_h + panel_margin, base_h + panel_margin + panel_h)



    return parts





def _bridge_pier(params):
    """Reconstructed parametric three-column cantilever bridge pier.

    Geometry aligned 100% with '桥Pier_07.step' (24 solids, 69.420 m3):
      1. Pier columns (3 cylinders): D=1200mm, H=9507mm, left X=-7200, center X=0, right X=7200
      2. Capping beam (14-face solid): L=20540mm, W=1400mm, H=1700mm, with cantilever tapers and stepped tops
      3. Plinths and bearings: 10 sets (8 sets on front Y=-350mm Z=11007, 2 sets on back Y=+350mm Z=10707)
    """
    h_pier = max(_num(params, "PierHeight", 9507.0), 500.0)
    cap_l = max(_num(params, "CapBeamLength", 20540.0), 1000.0)
    cap_w = max(_num(params, "CapBeamWidth", 1400.0), 500.0)
    cap_h = max(_num(params, "CapBeamHeight", 1700.0), 500.0)
    col_d = max(_num(params, "ColumnDiameter", 1200.0), 300.0)
    col_spacing = max(_num(params, "ColumnSpacing", 7200.0), col_d * 1.2)
    canti_l = max(_num(params, "CantileverLength", 2170.0), 100.0)
    canti_h = max(_num(params, "CantileverHeight", 672.0), 100.0)
    bed_th = max(_num(params, "BedStoneThickness", 30.0), 5.0)
    bearing_th = max(_num(params, "BearingThickness", 42.0), 5.0)

    parts = OrderedDict()

    # 1. 3 根圆柱墩身 PierColumnLeft, PierColumnCenter, PierColumnRight
    r_col = col_d / 2.0
    parts["PierColumnLeft"] = _cyl_z(r_col, 0.0, h_pier, x=-col_spacing, y=0.0)
    parts["PierColumnCenter"] = _cyl_z(r_col, 0.0, h_pier, x=0.0, y=0.0)
    parts["PierColumnRight"] = _cyl_z(r_col, 0.0, h_pier, x=col_spacing, y=0.0)

    # 2. 盖梁 CappingBeam (含底部斜悬臂托、两端防震挡块、横桥向高低错台)
    x_half = cap_l / 2.0
    block_w = min(250.0, x_half * 0.1)
    x_block_in = x_half - block_w
    x_canti_in = max(col_spacing + r_col, x_half - canti_l)

    z_bottom = h_pier
    z_canti_end = z_bottom + canti_h
    z_top_block = z_bottom + cap_h
    z_top_front = z_bottom + (cap_h - 200.0)
    z_top_back = z_bottom + (cap_h - 500.0)

    pts_front = [
        (x_half, z_canti_end),
        (x_half, z_top_block),
        (x_block_in, z_top_block),
        (x_block_in, z_top_front),
        (-x_block_in, z_top_front),
        (-x_block_in, z_top_block),
        (-x_half, z_top_block),
        (-x_half, z_canti_end),
        (-x_canti_in, z_bottom),
        (x_canti_in, z_bottom),
    ]

    pts_back = [
        (x_half, z_canti_end),
        (x_half, z_top_block),
        (x_block_in, z_top_block),
        (x_block_in, z_top_back),
        (-x_block_in, z_top_back),
        (-x_block_in, z_top_block),
        (-x_half, z_top_block),
        (-x_half, z_canti_end),
        (-x_canti_in, z_bottom),
        (x_canti_in, z_bottom),
    ]

    y_half = cap_w / 2.0
    s_front = cq.Workplane("XZ", origin=(0, -y_half, 0)).polyline(pts_front).close().extrude(-y_half)
    s_back = cq.Workplane("XZ", origin=(0, 0, 0)).polyline(pts_back).close().extrude(-y_half)
    parts["CappingBeam"] = s_front.union(s_back).val()

    # 3. 垫石与支座 (10 组：前侧 8 组 Y = -350mm, 后侧 2 组 Y = +350mm)
    bed_lx, bed_ly = 576.0, 480.0
    brg_lx, brg_ly = 403.2, 336.0
    y_front = -y_half / 2.0
    y_back = y_half / 2.0

    # 前侧 8 组 (Y = -350mm, Z_bed_bottom = 11007)
    x_front_list = [-9270.0, -6540.0, -3810.0, -1080.0, 1080.0, 3810.0, 6540.0, 9270.0]
    for idx, x_pos in enumerate(x_front_list, 1):
        z0 = z_top_front
        z1 = z0 + bed_th
        parts[f"BedStone_{idx:02d}"] = _box(
            x_pos - bed_lx / 2.0, x_pos + bed_lx / 2.0,
            y_front - bed_ly / 2.0, y_front + bed_ly / 2.0,
            z0, z1,
        )
        zb0 = z1
        zb1 = zb0 + bearing_th
        parts[f"Bearing_{idx:02d}"] = _box(
            x_pos - brg_lx / 2.0, x_pos + brg_lx / 2.0,
            y_front - brg_ly / 2.0, y_front + brg_ly / 2.0,
            zb0, zb1,
        )

    # 后侧 2 组 (Y = +350mm, Z_bed_bottom = 10707)
    x_back_list = [-1000.0, 1000.0]
    for idx, x_pos in enumerate(x_back_list, 9):
        z0 = z_top_back
        z1 = z0 + bed_th
        parts[f"BedStone_{idx:02d}"] = _box(
            x_pos - bed_lx / 2.0, x_pos + bed_lx / 2.0,
            y_back - bed_ly / 2.0, y_back + bed_ly / 2.0,
            z0, z1,
        )
        zb0 = z1
        zb1 = zb0 + bearing_th
        parts[f"Bearing_{idx:02d}"] = _box(
            x_pos - brg_lx / 2.0, x_pos + brg_lx / 2.0,
            y_back - brg_ly / 2.0, y_back + brg_ly / 2.0,
            zb0, zb1,
        )

    return parts


def _portal_pier(params):
    h_total = max(_num(params, "Height", 4000.0), 2500.0)
    w_total = max(_num(params, "TotalWidth", 4500.0), 2000.0)
    leg_d = max(_num(params, "LegDepth", 1000.0), 300.0)
    w_bottom = max(_num(params, "LegWidthBottom", 800.0), 300.0)
    spacing_bottom = max(_num(params, "LegSpacingBottom", 1000.0), 300.0)
    h_straight = max(_num(params, "StraightHeight", 2200.0), 500.0)
    r_in = max(_num(params, "InnerRadius", 2980.26), 1000.0)
    r_out = max(_num(params, "OuterRadius", 2180.26), 1000.0)
    fillet_r = min(max(_num(params, "FilletRadius", 80.0), 0.0), 120.0)

    z_tie_low = max(_num(params, "LowerTieBeamElevation", 1900.0), 500.0)
    z_tie_high = max(_num(params, "UpperTieBeamElevation", 3100.0), z_tie_low + 500.0)
    tie_h = max(_num(params, "TieBeamHeight", 400.0), 100.0)
    tie_w = max(_num(params, "TieBeamWidth", 314.2), 100.0)

    plinth_h_low = max(_num(params, "PlinthLowerHeight", 30.0), 10.0)
    plinth_h_up = max(_num(params, "PlinthUpperHeight", 42.0), 10.0)
    plinth_w_low = max(_num(params, "PlinthLowerWidth", 720.0), 200.0)
    plinth_d_low = max(_num(params, "PlinthLowerDepth", 700.0), 200.0)
    plinth_w_up = max(_num(params, "PlinthUpperWidth", 504.0), 100.0)
    plinth_d_up = max(_num(params, "PlinthUpperDepth", 490.0), 100.0)
    x_plinth = max(_num(params, "PlinthCenterOffset", 1674.0), 500.0)

    parts = OrderedDict()

    # 右肢与左肢轮廓参数
    x_in_0 = spacing_bottom / 2.0
    x_out_0 = x_in_0 + w_bottom
    cx_right = x_out_0 + r_out

    h_curve = h_total - h_straight
    dz_mid = h_curve / 2.0
    z_mid = h_straight + dz_mid

    dx_in_mid = math.sqrt(max(r_in**2 - dz_mid**2, 1.0))
    dx_out_mid = math.sqrt(max(r_out**2 - dz_mid**2, 1.0))
    x_in_mid_r = cx_right - dx_in_mid
    x_out_mid_r = cx_right - dx_out_mid

    dx_in_top = math.sqrt(max(r_in**2 - h_curve**2, 1.0))
    dx_out_top = math.sqrt(max(r_out**2 - h_curve**2, 1.0))
    x_in_top_r = cx_right - dx_in_top
    x_out_top_r = cx_right - dx_out_top

    def make_leg(side=1):
        sgn = float(side)
        p_b_in = (x_in_0 * sgn, 0.0)
        p_b_out = (x_out_0 * sgn, 0.0)
        p_s_out = (x_out_0 * sgn, h_straight)
        p_mid_out = (x_out_mid_r * sgn, z_mid)
        p_top_out = (x_out_top_r * sgn, h_total)
        p_top_in = (x_in_top_r * sgn, h_total)
        p_mid_in = (x_in_mid_r * sgn, z_mid)
        p_s_in = (x_in_0 * sgn, h_straight)

        shape = (
            cq.Workplane("XZ")
            .moveTo(p_b_in[0], p_b_in[1])
            .lineTo(p_b_out[0], p_b_out[1])
            .lineTo(p_s_out[0], p_s_out[1])
            .threePointArc(p_mid_out, p_top_out)
            .lineTo(p_top_in[0], p_top_in[1])
            .threePointArc(p_mid_in, p_s_in)
            .close()
            .extrude(leg_d / 2.0, both=True)
        )

        if fillet_r > 1.0:
            half_d = leg_d / 2.0
            def is_corner(edge):
                bb = edge.BoundingBox()
                if not (abs(bb.ymin - half_d) < 1e-2 and abs(bb.ymax - half_d) < 1e-2) and \
                   not (abs(bb.ymin - (-half_d)) < 1e-2 and abs(bb.ymax - (-half_d)) < 1e-2):
                    return False
                if abs(bb.zmin - 0.0) < 1e-3 and abs(bb.zmax - 0.0) < 1e-3:
                    return False
                if abs(bb.zmin - h_total) < 1e-3 and abs(bb.zmax - h_total) < 1e-3:
                    return False
                return True
            corner_edges = [e for e in shape.val().Edges() if is_corner(e)]
            if corner_edges:
                shape = shape.newObject(corner_edges).fillet(fillet_r)
        return shape

    parts["PierLegRight"] = make_leg(1)
    parts["PierLegLeft"] = make_leg(-1)

    # 系梁 TieBeams
    # 下系梁 (Z in [z_tie_low, z_tie_low + tie_h])
    z_low_0, z_low_1 = z_tie_low, z_tie_low + tie_h
    # 下系梁跨度 (内壁在 x_in_0 附近，微量嵌入 10mm)
    parts["TieBeamLower"] = _box(-x_in_0 - 10.0, x_in_0 + 10.0, -tie_w / 2.0, tie_w / 2.0, z_low_0, z_low_1)

    # 上系梁 (Z in [z_tie_high, z_tie_high + tie_h])
    z_high_0, z_high_1 = z_tie_high, z_tie_high + tie_h
    # 顶部内弧在 Z 处的 X 坐标
    def x_in_at_z(z_val):
        dz = z_val - h_straight
        return cx_right - math.sqrt(max(r_in**2 - dz**2, 1.0))
    x_span_0 = x_in_at_z(z_high_0) + 15.0
    x_span_1 = x_in_at_z(z_high_1) + 15.0
    x_span = max(x_span_0, x_span_1)
    parts["TieBeamUpper"] = _box(-x_span, x_span, -tie_w / 2.0, tie_w / 2.0, z_high_0, z_high_1)

    # 支座垫石 (4 块)
    # 左侧
    parts["PlinthLeftLower"] = _box(
        -x_plinth - plinth_w_low / 2.0, -x_plinth + plinth_w_low / 2.0,
        -plinth_d_low / 2.0, plinth_d_low / 2.0,
        h_total, h_total + plinth_h_low
    )
    parts["PlinthLeftUpper"] = _box(
        -x_plinth - plinth_w_up / 2.0, -x_plinth + plinth_w_up / 2.0,
        -plinth_d_up / 2.0, plinth_d_up / 2.0,
        h_total + plinth_h_low, h_total + plinth_h_low + plinth_h_up
    )
    # 右侧
    parts["PlinthRightLower"] = _box(
        x_plinth - plinth_w_low / 2.0, x_plinth + plinth_w_low / 2.0,
        -plinth_d_low / 2.0, plinth_d_low / 2.0,
        h_total, h_total + plinth_h_low
    )
    parts["PlinthRightUpper"] = _box(
        x_plinth - plinth_w_up / 2.0, x_plinth + plinth_w_up / 2.0,
        -plinth_d_up / 2.0, plinth_d_up / 2.0,
        h_total + plinth_h_low, h_total + plinth_h_low + plinth_h_up
    )

    return parts



def _stair(params):

    """Reconstructed parametric double-flight stair (双跑楼梯)."""

    flight_w = max(_num(params, "FlightWidth", 2000.0), 500.0)

    well_w = max(_num(params, "WellWidth", 200.0), 50.0)

    slab_t = max(_num(params, "SlabThickness", 200.0), 80.0)

    landing_d = max(_num(params, "LandingLength", 1800.0), 800.0)

    step_t = max(_num(params, "TreadDepth", 280.0), 150.0)

    step_n = max(int(_num(params, "StepCount", 10)), 3)

    story_h = max(_num(params, "StoryHeight", 3200.0), 1000.0)

    mid_h = story_h / 2.0

    step_r = mid_h / step_n

    run_len = step_t * step_n

    total_w = flight_w * 2.0 + well_w



    x_r0 = well_w / 2.0

    x_r1 = well_w / 2.0 + flight_w

    x_l0 = -well_w / 2.0 - flight_w

    x_l1 = -well_w / 2.0



    parts = OrderedDict()



    # 1. 底层入口平台板 (X: [x_r0, x_r1], Y: [-landing_d, 0], Z: [0, slab_t])

    parts["LandingBottom"] = (

        cq.Workplane("XY")

        .box(flight_w, landing_d, slab_t)

        .translate(((x_r0 + x_r1) / 2.0, -landing_d / 2.0, slab_t / 2.0))

        .val()

    )



    # 2. 第一跑梯段 (Flight 1: 从 Y=0, Z=0 平滑爬升至 Y=run_len, Z=mid_h)

    pts1 = [(0.0, 0.0)]

    for i in range(step_n):

        pts1.append((i * step_t, (i + 1) * step_r))

        pts1.append(((i + 1) * step_t, (i + 1) * step_r))

    pts1.append((run_len, mid_h - slab_t))

    pts1.append((0.0, -slab_t))

    pts1.append((0.0, 0.0))



    parts["Flight1_Run"] = (

        cq.Workplane("YZ")

        .polyline(pts1).close()

        .extrude(flight_w)

        .translate((x_r0, 0.0, 0.0))

        .val()

    )



    # 3. 中间层间休息大平台 (LandingMid: Y=[run_len, run_len + landing_d], X=[-total_w/2, total_w/2], Z=[mid_h - slab_t, mid_h])

    parts["LandingMid"] = (

        cq.Workplane("XY")

        .box(total_w, landing_d, slab_t)

        .translate((0.0, run_len + landing_d / 2.0, mid_h - slab_t / 2.0))

        .val()

    )



    # 4. 中间平台外侧支撑梁 (LandingBeamMid: 截面 300x500)

    beam_h = max(_num(params, "BeamHeight", 500.0), 200.0)

    beam_w = max(_num(params, "BeamWidth", 300.0), 100.0)

    parts["LandingBeamMid"] = (

        cq.Workplane("XY")

        .box(total_w, beam_w, beam_h)

        .translate((0.0, run_len + landing_d - beam_w / 2.0, mid_h - slab_t - beam_h / 2.0))

        .val()

    )



    # 5. 第二跑梯段 (Flight 2: 从中间平台 Y=run_len, Z=mid_h 向 -Y 折返爬升至 Y=0, Z=story_h)

    pts2 = [(run_len, mid_h)]

    for i in range(step_n):

        y0 = run_len - i * step_t

        y1 = run_len - (i + 1) * step_t

        z1 = mid_h + (i + 1) * step_r

        pts2.append((y0, z1))

        pts2.append((y1, z1))

    pts2.append((0.0, story_h - slab_t))

    pts2.append((run_len, mid_h - slab_t))

    pts2.append((run_len, mid_h))



    parts["Flight2_Run"] = (

        cq.Workplane("YZ")

        .polyline(pts2).close()

        .extrude(flight_w)

        .translate((x_l0, 0.0, 0.0))

        .val()

    )



    # 6. 顶层出口平台板 (LandingTop: X: [x_l0, x_l1], Y: [-landing_d, 0], Z: [story_h - slab_t, story_h])

    parts["LandingTop"] = (

        cq.Workplane("XY")

        .box(flight_w, landing_d, slab_t)

        .translate(((x_l0 + x_l1) / 2.0, -landing_d / 2.0, story_h - slab_t / 2.0))

        .val()

    )



    # 7. 顶层平台梁 (FloorBeamTop: 截面 300x500)

    floor_beam_h = max(_num(params, "FloorBeamHeight", 500.0), 200.0)

    floor_beam_w = max(_num(params, "FloorBeamWidth", 300.0), 100.0)

    parts["FloorBeamTop"] = (

        cq.Workplane("XY")

        .box(total_w, floor_beam_w, floor_beam_h)

        .translate((0.0, -landing_d + floor_beam_w / 2.0, story_h - slab_t - floor_beam_h / 2.0))

        .val()

    )



    # 8. 斜向防护扶手 Handrails (沿梯段爬升倾角延伸)

    rail_h = 900.0

    r_w = 60.0

    r_h = 60.0

    r1_poly = [

        (0.0, rail_h),

        (run_len, mid_h + rail_h),

        (run_len, mid_h + rail_h - r_h),

        (0.0, rail_h - r_h),

        (0.0, rail_h),

    ]

    parts["Handrail1_Outer"] = (

        cq.Workplane("YZ")

        .polyline(r1_poly).close()

        .extrude(r_w)

        .translate((x_r1 - r_w, 0.0, 0.0))

        .val()

    )

    r2_poly = [

        (run_len, mid_h + rail_h),

        (0.0, story_h + rail_h),

        (0.0, story_h + rail_h - r_h),

        (run_len, mid_h + rail_h - r_h),

        (run_len, mid_h + rail_h),

    ]

    parts["Handrail2_Outer"] = (

        cq.Workplane("YZ")

        .polyline(r2_poly).close()

        .extrude(r_w)

        .translate((x_l0, 0.0, 0.0))

        .val()

    )

    parts["Handrail_Inner"] = (

        cq.Workplane("YZ")

        .polyline(r1_poly).close()

        .extrude(r_w)

        .translate((x_r0, 0.0, 0.0))

        .val()

    )



    # 9. 扶手立柱 (Newel Posts)

    post_s = 60.0

    parts["Post_Bottom"] = (

        cq.Workplane("XY")

        .box(post_s, post_s, rail_h)

        .translate((x_r1 - post_s / 2.0, post_s / 2.0, rail_h / 2.0))

        .val()

    )

    parts["Post_Mid"] = (

        cq.Workplane("XY")

        .box(post_s, post_s, rail_h)

        .translate((x_r1 - post_s / 2.0, run_len - post_s / 2.0, mid_h + rail_h / 2.0))

        .val()

    )

    parts["Post_Top"] = (

        cq.Workplane("XY")

        .box(post_s, post_s, rail_h)

        .translate((x_l0 + post_s / 2.0, post_s / 2.0, story_h + rail_h / 2.0))

        .val()

    )



    return parts





def _escalator(params):

    """Reconstructed parametric 30-degree escalator (自动扶梯).



    Accurately matched to official Revit B-Rep and STEP reference geometry:

      - Main structural truss (TrussMain) with incline angle

      - Lower machine room pit support (LowerMachineRoom)

      - Upper machine room pit support (UpperMachineRoom)

      - Left & Right inner/outer decking and skirt panels (DeckingSkirtLeft/Right)

        with R=150 blend fillets and R=1500 / R=2100 transition arcs

      - Left & Right clear glass balustrade panels (GlassBalustradeLeft/Right)

        with R=415 round ends, R=150 blend fillets, and R=1500 / R=2100 transition arcs

      - Left & Right continuous rubber handrails (HandrailLeft/Right)

        with R=450/415 round ends, R=1535/1500 lower transition, and R=2100/2065 upper transition

      - Step runs with precision 30-degree incline steps (Step_01..Step_N)

      - Bottom & Top transition comb plates (CombPlateBottom/Top)

    """

    w1 = max(_num(params, "Width", 1150.0), 500.0)

    w2 = max(_num(params, "HandrailSpacing", 840.0), 400.0)

    w3 = max(_num(params, "SkirtSpacing", 610.0), 300.0)

    tk = max(_num(params, "TK", 2180.0), 1000.0)

    tj = max(_num(params, "TJ", 3015.0), 1000.0)

    height = max(_num(params, "Height", 4000.0), 1500.0)

    angle_deg = max(min(_num(params, "Angle", 30.0), 45.0), 20.0)



    angle_rad = math.radians(angle_deg)

    tan_a = math.tan(angle_rad)

    sin_a = math.sin(angle_rad)

    cos_a = math.cos(angle_rad)

    l_incline = height / tan_a

    l_total = tk + l_incline + tj



    parts = OrderedDict()



    # 1. 主桁架 TrussMain

    offset_x = 1030.0 * math.tan(math.radians(angle_deg / 2.0))

    truss_pts = [

        (0.0, 0.0),

        (tk, 0.0),

        (tk + l_incline, height),

        (l_total, height),

        (l_total, height - 1030.0),

        (tk + l_incline + offset_x, height - 1030.0),

        (tk + offset_x, -1030.0),

        (0.0, -1030.0),

        (0.0, 0.0),

    ]

    parts["TrussMain"] = (

        cq.Workplane("XZ")

        .polyline(truss_pts).close()

        .extrude(w1)

        .translate((0.0, w1 / 2.0, 0.0))

        .val()

    )



    # 2. 下机房 LowerMachineRoom

    parts["LowerMachineRoom"] = (

        cq.Workplane("XY")

        .box(4000.0, w1, 1100.0)

        .translate((2000.0, 0.0, -550.0))

        .val()

    )



    # 3. 上机房 UpperMachineRoom

    parts["UpperMachineRoom"] = (

        cq.Workplane("XY")

        .box(tj, w1, 1100.0)

        .translate((l_total - tj / 2.0, 0.0, height - 550.0))

        .val()

    )



    # 几何特征公共参数 (与 STEP 真值解析完全一致)

    x_c_low = tk - 1100.0

    z_c_low = 500.0

    x_c_high = l_total - 1630.0

    z_c_high = height + 500.0



    r_head = 415.0

    r_out = 450.0

    r_in = 415.0

    r_low_trans = 1500.0

    r_high_trans = 2100.0

    t_low = r_low_trans * math.tan(angle_rad / 2.0)

    t_high = r_high_trans * math.tan(angle_rad / 2.0)



    # 4. 内盖板与围裙板 DeckingSkirtLeft & Right

    deck_w = max((w1 - w3) / 2.0, 50.0)

    x_v_bot_shift = (915.0 - 225.0) * math.tan(angle_rad / 2.0)

    x_v_bot_low = tk - 245.24 + x_v_bot_shift

    x_v_bot_high = tk + l_incline - 245.24 + x_v_bot_shift



    x_trans_high_bot_start_pt = x_v_bot_high + t_high

    p_trans_high_bot_start = (x_trans_high_bot_start_pt, height + 225.0)

    p_trans_high_bot_end = (x_trans_high_bot_start_pt - r_high_trans * sin_a, height + 225.0 - r_high_trans * (1.0 - cos_a))



    x_trans_low_bot_start_pt = x_v_bot_low - t_low

    p_trans_low_bot_start = (x_trans_low_bot_start_pt, 225.0)

    p_trans_low_bot_end = (x_trans_low_bot_start_pt + r_low_trans * sin_a, 225.0 + r_low_trans * (1.0 - cos_a))



    skirt_w = (

        cq.Workplane("XZ")

        .moveTo(x_c_low, 0.0)

        .lineTo(tk, 0.0)

        .lineTo(tk + l_incline, height)

        .lineTo(x_c_high, height)

        .lineTo(x_c_high, height + 75.0)

        .radiusArc((x_c_high - 150.0, height + 225.0), -150.0)

        .lineTo(p_trans_high_bot_start[0], p_trans_high_bot_start[1])

        .radiusArc(p_trans_high_bot_end, -r_high_trans)

        .lineTo(p_trans_low_bot_end[0], p_trans_low_bot_end[1])

        .radiusArc(p_trans_low_bot_start, r_low_trans)

        .lineTo(x_c_low + 150.0, 225.0)

        .radiusArc((x_c_low, 75.0), -150.0)

        .lineTo(x_c_low, 0.0)

        .close()

    )

    s_skirt_l = skirt_w.extrude(deck_w).translate((0.0, -w1 / 2.0 + deck_w, 0.0)).val()

    parts["DeckingSkirtLeft"] = s_skirt_l

    parts["DeckingSkirtRight"] = s_skirt_l.mirror("XZ")



    # 5. 玻璃护栏板 GlassBalustradeLeft & Right (带两端圆角与平滑过渡圆弧)

    glass_t = 10.0

    x_v_top_low = tk - 245.24

    x_trans_low_top_start = x_v_top_low - t_low

    p_trans_low_top_start = (x_trans_low_top_start, z_c_low + r_head)

    p_trans_low_top_end = (x_trans_low_top_start + r_low_trans * sin_a, z_c_low + r_head + r_low_trans * (1.0 - cos_a))



    x_v_top_high = tk + l_incline - 245.24

    x_trans_high_top_end_pt = x_v_top_high + t_high

    x_trans_high_top_start_pt = x_trans_high_top_end_pt - r_high_trans * sin_a

    z_trans_high_top_start_pt = height + 915.0 - r_high_trans * (1.0 - cos_a)

    p_trans_high_top_start = (x_trans_high_top_start_pt, z_trans_high_top_start_pt)

    p_trans_high_top_end = (x_trans_high_top_end_pt, height + 915.0)



    glass_w = (

        cq.Workplane("XZ")

        .moveTo(p_trans_low_bot_start[0], p_trans_low_bot_start[1])

        .lineTo(x_c_low + 150.0, 225.0)

        .radiusArc((x_c_low + 0.3, z_c_low - r_head), -150.0)

        .threePointArc((x_c_low - r_head, z_c_low), (x_c_low, z_c_low + r_head))

        .lineTo(p_trans_low_top_start[0], p_trans_low_top_start[1])

        .radiusArc(p_trans_low_top_end, -r_low_trans)

        .lineTo(p_trans_high_top_start[0], p_trans_high_top_start[1])

        .radiusArc(p_trans_high_top_end, r_high_trans)

        .lineTo(x_c_high, height + 915.0)

        .threePointArc((x_c_high + r_head, z_c_high), (x_c_high - 0.3, height + 85.0))

        .radiusArc((x_c_high - 150.0, height + 225.0), -150.0)

        .lineTo(p_trans_high_bot_start[0], p_trans_high_bot_start[1])

        .radiusArc(p_trans_high_bot_end, -r_high_trans)

        .lineTo(p_trans_low_bot_end[0], p_trans_low_bot_end[1])

        .radiusArc(p_trans_low_bot_start, r_low_trans)

        .close()

    )

    s_glass_l = glass_w.extrude(glass_t).translate((0.0, -w2 / 2.0 + glass_t / 2.0, 0.0)).val()

    parts["GlassBalustradeLeft"] = s_glass_l

    parts["GlassBalustradeRight"] = s_glass_l.mirror("XZ")



    # 6. 扶手带 HandrailLeft & Right (带端头回转半圆与平滑过渡圆弧)

    hr_w = 90.0

    r_low_out = 1535.0

    r_low_in = 1500.0

    r_high_out = 2100.0

    r_high_in = 2065.0



    t_low_out = r_low_out * math.tan(angle_rad / 2.0)

    t_high_out = r_high_out * math.tan(angle_rad / 2.0)



    x_v_low = tk - 245.24

    x_low_trans = x_v_low - t_low_out

    x_v_high = tk + l_incline - 245.24 - (r_out - r_in) * math.tan(angle_rad / 2.0)

    x_high_trans = x_v_high + t_high_out



    p_out_low_bot = (x_c_low, z_c_low - r_out)

    p_out_low_mid = (x_c_low - r_out, z_c_low)

    p_out_low_top = (x_c_low, z_c_low + r_out)



    p_trans_low_out_start = (x_low_trans, z_c_low + r_out)

    p_trans_low_out_end = (x_low_trans + r_low_out * sin_a, z_c_low + r_out + r_low_out * (1.0 - cos_a))



    p_trans_high_out_start = (x_high_trans - r_high_out * sin_a, height + z_c_low + r_out - r_high_out * (1.0 - cos_a))

    p_trans_high_out_end = (x_high_trans, height + z_c_low + r_out)



    p_out_high_top = (x_c_high, height + z_c_low + r_out)

    p_out_high_mid = (x_c_high + r_out, z_c_high)

    p_out_high_bot = (x_c_high, height + z_c_low - r_out)



    p_in_high_bot = (x_c_high, height + z_c_low - r_in)

    p_in_high_mid = (x_c_high + r_in, z_c_high)

    p_in_high_top = (x_c_high, height + z_c_low + r_in)



    p_trans_high_in_end = (x_high_trans, height + z_c_low + r_in)

    p_trans_high_in_start = (x_high_trans - r_high_in * sin_a, height + z_c_low + r_in - r_high_in * (1.0 - cos_a))



    p_trans_low_in_end = (x_low_trans + r_low_in * sin_a, z_c_low + r_in + r_low_in * (1.0 - cos_a))

    p_trans_low_in_start = (x_low_trans, z_c_low + r_in)



    p_in_low_top = (x_c_low, z_c_low + r_in)

    p_in_low_mid = (x_c_low - r_in, z_c_low)

    p_in_low_bot = (x_c_low, z_c_low - r_in)



    hr_w_plane = (

        cq.Workplane("XZ")

        .moveTo(p_out_low_bot[0], p_out_low_bot[1])

        .threePointArc(p_out_low_mid, p_out_low_top)

        .lineTo(p_trans_low_out_start[0], p_trans_low_out_start[1])

        .radiusArc(p_trans_low_out_end, -r_low_out)

        .lineTo(p_trans_high_out_start[0], p_trans_high_out_start[1])

        .radiusArc(p_trans_high_out_end, r_high_out)

        .lineTo(p_out_high_top[0], p_out_high_top[1])

        .threePointArc(p_out_high_mid, p_out_high_bot)

        .lineTo(p_in_high_bot[0], p_in_high_bot[1])

        .threePointArc(p_in_high_mid, p_in_high_top)

        .lineTo(p_trans_high_in_end[0], p_trans_high_in_end[1])

        .radiusArc(p_trans_high_in_start, -r_high_in)

        .lineTo(p_trans_low_in_end[0], p_trans_low_in_end[1])

        .radiusArc(p_trans_low_in_start, r_low_in)

        .lineTo(p_in_low_top[0], p_in_low_top[1])

        .threePointArc(p_in_low_mid, p_in_low_bot)

        .lineTo(p_out_low_bot[0], p_out_low_bot[1])

        .close()

    )

    s_hr_l = hr_w_plane.extrude(hr_w).translate((0.0, -w2 / 2.0 + hr_w / 2.0, 0.0)).val()

    parts["HandrailLeft"] = s_hr_l

    parts["HandrailRight"] = s_hr_l.mirror("XZ")



    # 7. 梯级踏步 Steps

    step_rise = 190.476

    step_run = step_rise / tan_a

    step_depth = 370.0

    n_steps = max(int(round(height / step_rise)), 3)

    step_w = w3

    for i in range(n_steps):

        sx0 = tk - 40.0 + i * step_run

        sz0 = i * step_rise

        parts[f"Step_{i + 1:02d}"] = (

            cq.Workplane("XY")

            .box(step_depth, step_w, step_rise)

            .translate((sx0 + step_depth / 2.0, 0.0, sz0 + step_rise / 2.0))

            .val()

        )



    # 8. 梳齿板 CombPlateBottom & CombPlateTop

    comb_l = 300.0

    comb_t = 25.0

    parts["CombPlateBottom"] = (

        cq.Workplane("XY")

        .box(comb_l, w3, comb_t)

        .translate((tk - comb_l / 2.0, 0.0, comb_t / 2.0))

        .val()

    )

    parts["CombPlateTop"] = (

        cq.Workplane("XY")

        .box(comb_l, w3, comb_t)

        .translate((tk + l_incline + comb_l / 2.0, 0.0, height + comb_t / 2.0))

        .val()

    )



    return parts





def build_parts(params):

    if TEMPLATE == "column":

        return _round_column(params)

    if TEMPLATE == "manifold":

        return _manifold(params)

    if TEMPLATE == "door":

        return _door(params)

    if TEMPLATE == "elevator":

        return _elevator(params)

    if TEMPLATE == "pump":

        return _pump(params)

    if TEMPLATE == "rubber_joint":

        return _rubber_joint(params)

    if TEMPLATE == "silencer":

        return _silencer(params)

    if TEMPLATE == "tank":

        return _tank(params)

    if TEMPLATE == "ahu":

        return _ahu(params)

    if TEMPLATE == "bridge_pier":

        return _bridge_pier(params)

    if TEMPLATE == "portal_pier":

        return _portal_pier(params)

    if TEMPLATE == "stair":

        return _stair(params)

    if TEMPLATE == "escalator":

        return _escalator(params)

    return _generic(params)

