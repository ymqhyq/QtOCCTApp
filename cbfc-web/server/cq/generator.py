"""双线一字形桥台 · 参数化构件族生成器 (CadQuery) —— Round 2 重构版

依据：中国铁设《桥通（2022）4332A-Ⅰ》双线一字形桥台通用图
      （跨度：24m(高)、32m 无砟轨道双线简支箱梁，直/曲线，V=350 km/h）

Round 2 修正要点（对应 docs/abutment_family_review.md）：
    B1 改为「台阶状支撑体系 + 胸墙」构型：台身前部为顶帽支撑台阶（低标高、承托垫石），
       台身后部为高 h0=3631mm 的胸墙，台顶托盘仅坐落于胸墙顶端向外悬挑；
    B2 台身高度严格等于表列 h2，顶帽 1050mm 独立成层，取消常量重复累加；
    B3 全台 2 个支承垫石（X=±2200），底面严密贴合顶帽台阶顶面；
    M2 按部位输出独立子形状（build_parts / build_assembly），不再只给一个融合实体；
    M3 胸墙正面中央开出 1500mm 宽检查口凹槽。

坐标约定（单位 mm）：
    X  横桥向，0 = 桥台中心线，+X 指向线路右侧
    Y  顺桥向，0 = 台身背面（台后侧），+Y 指向桥跨（台前）
    Z  竖直向，0 = 基础底面

标高链（与通用图 h0、hc、hs、hz 一致）：
    基础底 0 ─ 基顶 h1 ─ 台身顶 h1+h2 ─ 顶帽顶(垫石底) h1+h2+1050
           ─ 台顶(托盘顶) 顶帽顶+3631
    即：台顶 − 垫石顶 = 3281 = 梁高 3031 + 垫石顶至梁底 250；
        梁底 = 垫石顶 + 350(垫石) + 60(hz 余量) → 与 hz=60、h0=3631 自洽。
"""

from __future__ import annotations

from collections import OrderedDict

import cadquery as cq

# --------------------------------------------------------------------------
# 图纸常量（mm）
# --------------------------------------------------------------------------
BODY_WIDTH = 7800.0          # 台身/顶帽/胸墙横桥向宽度（通用图 780）
TOP_WIDTH = 12600.0          # 台顶托盘横桥向宽度（通用图 1260）
CAP_HEIGHT = 1050.0          # 顶帽（支撑台阶）高（通用图 105）
PARAPET_HEIGHT = 3631.0      # 台顶至顶帽顶 h0 = 363.1cm（桥台设计参数表）
# --- 顺桥向定位（Round 3：B4/M4 修正，以台身前面 y_front=o 为基准）---
CAP_FRONT_OH = 750.0         # 顶帽支撑台阶前缘挑出台身前面（700 挑出 + 50 襟边）
PAD_BACK_IN = 1300.0         # 支承垫石后缘缩进台身前面（通用图 130）
PAD_FRONT_OH = 700.0         # 支承垫石前缘挑出台身前面（通用图 70）
PARAPET_GAP = 1000.0         # 垫石后缘至胸墙前面的净距（构造图一 侧面量取 976mm）
TRAY_FRONT_OH = 250.0        # 台顶托盘前缘超出胸墙前面
TRAY_EDGE_T = 350.0          # 托盘边缘厚度（通用图 35）
TRAY_BODY_T = 615.0          # 托盘在胸墙处厚度（通用图 61.5）
FOOTING_OFFSET = 1050.0      # 基础襟边（正面轮廓 3900 -> 4950）
PAD_LEN = 2000.0             # 支承垫石顺桥向长度（通用图 130+70）
PAD_WID = 1300.0             # 支承垫石横桥向宽度
PAD_H = 350.0                # 支承垫石高度（通用图 e=35）
PAD_X = 2200.0               # 支座中心至桥台中心线（通用图 ±220）
WATER_CURB_H = 75.0          # 台顶挡水台高度（通用图 7.5(12)）
WATER_CURB_W = 250.0         # 台顶挡水台宽度
SLAB_NOTCH_W = 890.0         # 台后搭板预留槽宽（通用图 890(840)）
SLAB_NOTCH_D = 40.0          # 台后搭板预留槽深（通用图 40）
SLAB_NOTCH_L = 500.0         # 台后搭板预留槽顺桥向长度
INSPECT_W = 1500.0           # 胸墙正面检查口宽度（正面图 150）
INSPECT_D = 700.0            # 检查口凹槽深度（向后内凹）
BATTER_FRONT = 0.2           # 台身前面收坡 1:5（构造图一 侧面 "5:1"）

# 通用图《台身尺寸及工程数量表》参数规律
_O_BASE = 4.30
_O_PER_FILL = 0.20
_O_PER_DEPTH = 0.20

FAMILY_ID = "StraightAbutment"
FAMILY_NAME = "双线一字形桥台"

# 部位键（与 component.json 的 shapes 对应）
PART_KEYS = [
    "Footing", "Body", "Cap", "ParapetWall", "TopTray",
    "BearingPadLeft", "BearingPadRight",
    "WaterCurbA", "WaterCurbBLeft", "WaterCurbBRight",
]

# 部位语义与材质（不同材料组成部分在**同一个几何文件内**独立命名、独立着色）
#   key    = 生成器内部键
#   node   = 写入 STEP 产品名 / glTF node 名的命名几何锚点（CBF-C §4.1 <shapeKey>/<instanceTag>）
PART_META = OrderedDict([
    ("Footing", {"node": "Footing", "label": "基础", "material": "C35",
                 "code": "Bridge.Abutment.Footing"}),
    ("Body", {"node": "Body", "label": "台身", "material": "C35",
              "code": "Bridge.Abutment.Body"}),
    ("Cap", {"node": "Cap", "label": "顶帽支撑台阶", "material": "C35",
             "code": "Bridge.Abutment.Cap"}),
    ("ParapetWall", {"node": "ParapetWall", "label": "胸墙", "material": "C35",
                     "code": "Bridge.Abutment.ParaperWall"}),
    ("TopTray", {"node": "TopTray", "label": "台顶托盘", "material": "C35",
                 "code": "Bridge.Abutment.TopTray"}),
    ("BearingPadLeft", {"node": "BearingPad/Left", "label": "支承垫石(左)", "material": "C50",
                        "code": "Bridge.Abutment.BearingPad"}),
    ("BearingPadRight", {"node": "BearingPad/Right", "label": "支承垫石(右)", "material": "C50",
                         "code": "Bridge.Abutment.BearingPad"}),
    ("WaterCurbA", {"node": "WaterCurb/A", "label": "台前挡水台", "material": "C35",
                    "code": "Bridge.Abutment.WaterCurb"}),
    ("WaterCurbBLeft", {"node": "WaterCurb/B-Left", "label": "台后挡水台(左)", "material": "C35",
                        "code": "Bridge.Abutment.WaterCurb"}),
    ("WaterCurbBRight", {"node": "WaterCurb/B-Right", "label": "台后挡水台(右)", "material": "C35",
                         "code": "Bridge.Abutment.WaterCurb"}),
])

# 材质配色（查看器 / glTF 使用）
MATERIAL_COLORS = {
    "C35": (0.72, 0.71, 0.67),
    "C50": (0.55, 0.66, 0.80),
}


def derive_o(fill_height_m: float, embed_depth_m: float) -> float:
    """由填土高 H(表列 H+Δ) 与入土深 h1 推求台身顺桥向宽度 o（m）。"""
    return round(_O_BASE + _O_PER_FILL * (fill_height_m - 5.0) + _O_PER_DEPTH * embed_depth_m, 2)


def derive_h2(fill_height_m: float, embed_depth_m: float) -> float:
    """由填土高与入土深推求台身高度 h2（m）：h2 = h1 + (H+Δ-5.0)。"""
    return round(embed_depth_m + (fill_height_m - 5.0), 2)


# --------------------------------------------------------------------------
# 几何工具
# --------------------------------------------------------------------------
def _box(x0, x1, y0, y1, z0, z1) -> cq.Solid:
    return cq.Solid.makeBox(x1 - x0, y1 - y0, z1 - z0, cq.Vector(x0, y0, z0))


def _rect_wire(x0, x1, y0, y1, z) -> cq.Wire:
    pts = [
        cq.Vector(x0, y0, z),
        cq.Vector(x1, y0, z),
        cq.Vector(x1, y1, z),
        cq.Vector(x0, y1, z),
    ]
    return cq.Wire.makePolygon(pts + [pts[0]])


def _clean(shape: cq.Shape) -> cq.Shape:
    try:
        shape = shape.clean()
    except Exception:
        pass
    return shape


def _fuse(a: cq.Shape, b: cq.Shape) -> cq.Shape:
    out = a.fuse(b)
    if isinstance(out, cq.Compound):
        solids = out.Solids()
        if len(solids) == 1:
            out = solids[0]
    return out


def _loft(rect_bottom, rect_top) -> cq.Solid:
    wb = _rect_wire(*rect_bottom)
    wt = _rect_wire(*rect_top)
    return _clean(cq.Solid.makeLoft([wb, wt], ruled=True))


# --------------------------------------------------------------------------
# 标高体系
# --------------------------------------------------------------------------
def levels(params: dict) -> dict:
    """返回各控制标高（mm），供生成器与上层校验共用。"""
    h1 = float(params.get("EmbedDepth", 0.0)) * 1000.0
    fill = float(params.get("FillHeight", 5.0))
    o = float(params.get("BodyLength", derive_o(fill, float(params.get("EmbedDepth", 0.0))))) * 1000.0
    h2 = float(params.get("BodyHeight", derive_h2(fill, float(params.get("EmbedDepth", 0.0))))) * 1000.0
    z_body_top = h1 + h2
    z_cap_top = z_body_top + CAP_HEIGHT
    z_top = z_cap_top + PARAPET_HEIGHT
    return {
        "h1": h1, "h2": h2, "o": o,
        "z_footing_bottom": 0.0,
        "z_footing_top": h1,
        "z_body_top": z_body_top,
        "z_cap_top": z_cap_top,
        "z_top": z_top,
        "z_tray_bottom": z_top - TRAY_BODY_T,
        "z_pad_top": z_cap_top + PAD_H,
        "z_girder_bottom": z_cap_top + PAD_H + 60.0,
    }


# --------------------------------------------------------------------------
# 分部位构造
# --------------------------------------------------------------------------
def build_parts(params: dict) -> "OrderedDict[str, cq.Shape]":
    """按部位返回独立子形状（CBF-C shapes 槽位）。"""
    L = levels(params)
    h1, o, h2 = L["h1"], L["o"], L["h2"]
    z_body_top, z_cap_top, z_top = L["z_body_top"], L["z_cap_top"], L["z_top"]
    z_tray_bottom = L["z_tray_bottom"]

    x_body = BODY_WIDTH / 2.0
    x_top = TOP_WIDTH / 2.0
    y_back, y_front = 0.0, o
    y_bot_front = y_front - h2 * BATTER_FRONT     # 台身底部前面（1:5 收坡内移）

    # 顺桥向控制线（B4 / M4）
    y_cap_front = y_front + CAP_FRONT_OH          # 顶帽前缘：挑出 750（含 50 襟边）
    y_pad_back = y_front - PAD_BACK_IN            # 垫石后缘：缩进 1300
    y_pad_front = y_front + PAD_FRONT_OH          # 垫石前缘：挑出 700
    y_parapet_front = y_pad_back - PARAPET_GAP    # 胸墙前壁：紧随垫石后侧排水坡
    y_tray_front = y_parapet_front + TRAY_FRONT_OH  # 托盘前缘略超胸墙

    parts: "OrderedDict[str, cq.Shape]" = OrderedDict()

    # 1) 基础
    if h1 > 1e-6:
        fo = FOOTING_OFFSET
        parts["Footing"] = _box(-x_body - fo, x_body + fo,
                                y_back - fo, y_bot_front + fo,
                                L["z_footing_bottom"], L["z_footing_top"])
    else:
        parts["Footing"] = None  # h1=0：无明挖基础

    # 2) 台身（前面 1:5 收坡）
    if h2 > 1e-6 and BATTER_FRONT > 0:
        parts["Body"] = _loft(
            (-x_body, x_body, y_back, y_bot_front, L["z_footing_top"]),
            (-x_body, x_body, y_back, y_front, z_body_top),
        )
    else:
        parts["Body"] = _box(-x_body, x_body, y_back, y_front,
                             L["z_footing_top"], max(z_body_top, L["z_footing_top"] + 1.0))

    # 3) 顶帽支撑台阶（前伸挑出承托垫石；1050 厚即含 500 托台）
    parts["Cap"] = _box(-x_body, x_body, y_parapet_front, y_cap_front, z_body_top, z_cap_top)

    # 4) 胸墙（紧随垫石后方，厚度随 o 联动）
    parapet = _box(-x_body, x_body, y_back, y_parapet_front, z_body_top, z_tray_bottom)
    # M3：正面中央检查口凹槽（宽 1500，向后内凹 700，自顶帽顶起）
    recess = _box(-INSPECT_W / 2.0, INSPECT_W / 2.0,
                  y_parapet_front - INSPECT_D, y_parapet_front + 10.0,
                  z_cap_top, z_tray_bottom + 10.0)
    parts["ParapetWall"] = _clean(parapet.cut(recess))

    # 5) 台顶托盘（仅坐落于胸墙顶端，向外悬挑至 12600 宽）
    parts["TopTray"] = _fuse(
        _loft(
            (-x_body, x_body, y_back, y_parapet_front, z_tray_bottom),
            (-x_top, x_top, y_back, y_tray_front, z_top - TRAY_EDGE_T),
        ),
        _box(-x_top, x_top, y_back, y_tray_front, z_top - TRAY_EDGE_T, z_top),
    )

    # 6) 支承垫石（2 个，1300 落在顶帽台阶上 + 700 挑出，底面贴 z_cap_top）
    for key, sx in (("BearingPadLeft", -1.0), ("BearingPadRight", 1.0)):
        xc = sx * PAD_X
        parts[key] = _box(xc - PAD_WID / 2.0, xc + PAD_WID / 2.0,
                          y_pad_back, y_pad_front,
                          z_cap_top, z_cap_top + PAD_H)

    # 7) 台顶挡水台 A（托盘前缘）/ B（托盘后缘，被搭板槽断开）
    parts["WaterCurbA"] = _box(-x_top, x_top,
                               y_tray_front - WATER_CURB_W, y_tray_front,
                               z_top, z_top + WATER_CURB_H)
    curb_b = []
    for x0, x1 in ((-x_top, -SLAB_NOTCH_W / 2.0), (SLAB_NOTCH_W / 2.0, x_top)):
        curb_b.append(_box(x0, x1, y_back, y_back + WATER_CURB_W, z_top, z_top + WATER_CURB_H))
    # 台后挡水台被搭板槽断为左右两段，各自独立命名，避免一个部位内含多个游离实体
    parts["WaterCurbBLeft"], parts["WaterCurbBRight"] = curb_b[0], curb_b[1]

    # 台后搭板预留槽（在托盘顶面后缘）
    notch = _box(-SLAB_NOTCH_W / 2.0, SLAB_NOTCH_W / 2.0,
                 y_back, y_back + SLAB_NOTCH_L,
                 z_top - SLAB_NOTCH_D, z_top + 10.0)
    parts["TopTray"] = _clean(parts["TopTray"].cut(notch))

    return OrderedDict((k, parts[k]) for k in PART_KEYS if k in parts)


def build_abutment(params: dict) -> cq.Shape:
    """融合为单一实体（供 GLB 预览、BREP 交付、几何有效性校核）。"""
    parts = build_parts(params)
    body = None
    for shape in parts.values():
        if shape is None or shape.Volume() <= 1e-6:
            continue
        body = shape if body is None else _fuse(body, shape)
    return _clean(body) if body is not None else cq.Compound()


def build_assembly(params: dict) -> cq.Assembly:
    """按部位命名装配：写入 STEP 的产品名即 CBF-C 的命名几何锚点。

    交付的仍是**单个** geometry/default.step —— 各部位模型为该装配下的命名产品，
    component.json 的 shapes.<Key> 与之逐条对应（CBF-C v1.4.2 §3.1 / §4.1）。
    """
    assy = cq.Assembly(name="StraightAbutment")
    for key, shape in build_parts(params).items():
        if shape is None or shape.Volume() <= 1e-6:
            continue
        meta = PART_META[key]
        assy.add(shape, name=meta["node"])
    return assy


def parts_volume(params: dict) -> dict:
    out = {}
    for key, shape in build_parts(params).items():
        if shape is None:
            continue
        v = shape.Volume()
        out[key] = round(v / 1e9, 3) if v > 1e-6 else 0.0
    return out


def parts_compound(params: dict) -> cq.Compound:
    """多部位复合体（保持各部位为独立 Solid 与既有顺序，不融合）。"""
    comp = cq.Compound.makeCompound(
        [s for s in build_parts(params).values() if s is not None and s.Volume() > 1e-6]
    )
    return comp


def part_meshes(params: dict, tol: float = 4.0, angular: float = 0.3) -> list[dict]:
    """按部位输出三角网格（含命名锚点/材质/配色），供单文件多节点 GLB 与查看器使用。"""
    out = []
    for key, shape in build_parts(params).items():
        if shape is None or shape.Volume() <= 1e-6:
            continue
        verts, tris = shape.tessellate(tol, angular)
        meta = PART_META[key]
        out.append({
            "key": key,
            "name": meta["node"],
            "shapeKey": meta["node"].split("/")[0],
            "instanceTag": meta["node"].split("/")[1] if "/" in meta["node"] else None,
            "label": meta["label"],
            "material": meta["material"],
            "code": meta["code"],
            "volumeM3": round(shape.Volume() / 1e9, 3),
            "color": MATERIAL_COLORS[meta["material"]],
            "verts": [(v.x, v.y, v.z) for v in verts],
            "tris": tri_list(tris),
        })
    return out


def tri_list(tris) -> list:
    return [list(t) for t in tris]


# --------------------------------------------------------------------------
# 构件族元数据
# --------------------------------------------------------------------------
PARAM_SCHEMA = [
    {"name": "ModelNumber", "label": "标准型号", "type": "enum", "default": "A4332-H8.0-D0.0"},
    {"name": "FillHeight", "label": "填土高 H+Δ", "type": "real", "default": 8.0, "unit": "m",
     "min": 5.0, "max": 8.0, "step": 0.5},
    {"name": "EmbedDepth", "label": "台身入土深 h1", "type": "real", "default": 0.0, "unit": "m",
     "min": 0.0, "max": 3.0, "step": 0.5},
    {"name": "BodyHeight", "label": "台身高度 h2", "type": "real", "default": 3.0, "unit": "m",
     "min": 0.0, "max": 6.5, "step": 0.5, "access": "in"},
    {"name": "BodyLength", "label": "台身宽 o", "type": "real", "default": 4.90, "unit": "m",
     "min": 4.30, "max": 5.50, "step": 0.1, "access": "in"},
    {"name": "BodyWidth", "label": "台身横向宽", "type": "real", "default": 7800.0, "unit": "mm"},
    {"name": "TopWidth", "label": "台顶托盘宽", "type": "real", "default": 12600.0, "unit": "mm"},
    {"name": "NetVolume", "label": "主体体积", "type": "real", "default": 0.0, "unit": "m3", "access": "out"},
]


def build(params: dict | None = None) -> cq.Shape:
    """cbfc recipe 入口：入参为参数字典，返回桥台主体实体。"""
    return build_abutment(dict(params or {}))


if __name__ == "__main__":
    # 本地开发用（门禁 3 允许 show() 仅出现在 __main__ 块内）
    try:
        from ocp_vscode import show  # type: ignore
    except Exception:
        show = None
    p = {"FillHeight": 8.0, "EmbedDepth": 0.0}
    solid = build(p)
    print("volume m3 =", round(solid.Volume() / 1e9, 3))
    print("levels =", {k: round(v, 1) for k, v in levels(p).items()})
    print("parts =", parts_volume(p))
    if show:
        show(build_assembly(p))
