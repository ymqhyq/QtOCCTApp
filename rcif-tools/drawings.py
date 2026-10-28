"""标准图号登记表 (drawings.py)

PDF 摄取此前只认《桥通（2022）4332A-Ⅰ 双线一字形桥台》一张图（指纹与真值均硬编码），
遇到同目录下的其他标准图（如《通桥（2016）2322A-Ⅱ 预制双线简支箱梁》）会直接拒绝。
本模块把“图号 → 模板族 / 数量表真值 / 参数表”的映射收敛为一张登记表：

- 有文字层的 PDF：按 textTokens 正则匹配（与旧行为一致，4332A 优先命中原分支）；
- 纯扫描件（无文字层）：按文件名中的图号 token 匹配，并在元数据中如实标注
  `textLayer=false` 与 `fingerprintBasis=filename`；
- 登记表外不在这里静默匹配：`pdf2rcif.py` 会转到 `pdf_generic.py` 的通用体量草稿，
  明确标注低置信度并进入人工复核。

新增图纸只需在此登记：图号 token、模板族目录（含 generators/generator.py 与
table_truth.json）以及元数据，无需再改摄取与转换代码。
"""

from __future__ import annotations

import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# 4332A-I 桥台的参数证据（原 pdf_ingest.annotations 内容，逐条保留）
ABUTMENT_ANNOTATIONS = [
    {
        "name": "FillHeight",
        "evidence": {"page": 2, "snippet": "填土高度H+Δ (m): 5.0 ~ 8.0m", "bbox": [120, 240, 360, 280]},
        "status": "inferred",
    },
    {
        "name": "EmbedDepth",
        "evidence": {"page": 2, "snippet": "入土深度h1 (m): 0.0 ~ 3.0m", "bbox": [120, 290, 360, 330]},
        "status": "inferred",
    },
    {
        "name": "BodyWidth",
        "evidence": {"page": 3, "snippet": "台身横向总宽: 7.80m = 7800mm", "bbox": [450, 180, 680, 220]},
        "status": "ground_truth",
    },
    {
        "name": "TopWidth",
        "evidence": {"page": 3, "snippet": "台顶托盘总宽: 12.60m = 12600mm", "bbox": [450, 230, 680, 270]},
        "status": "ground_truth",
    },
]

BOX_GIRDER_ANNOTATIONS = [
    {
        "name": "SpanLength",
        "evidence": {"sheet": "Ⅱ-07 梁体", "snippet": "正立面总长 31500（31500/2=15750）"},
        "status": "ground_truth",
    },
    {
        "name": "DeckWidth",
        "evidence": {"sheet": "Ⅱ-07 梁体 Ⅰ-Ⅰ 截面", "snippet": "桥面全宽 12600（12600/2=6300）"},
        "status": "ground_truth",
    },
    {
        "name": "Height",
        "evidence": {"sheet": "Ⅱ-07 梁体 Ⅰ-Ⅰ 截面", "snippet": "梁高 3032（1920+…，截面全高）"},
        "status": "ground_truth",
    },
    {
        "name": "BottomWidth",
        "evidence": {"sheet": "Ⅱ-07 梁体 Ⅰ-Ⅰ 截面", "snippet": "底板宽 5500（5500/2=2750）"},
        "status": "ground_truth",
    },
    {
        "name": "NetVolume",
        "evidence": {"sheet": "Ⅱ-08 工程数量表", "snippet": "主梁 C50 混凝土 316.1 m³/孔"},
        "status": "ground_truth",
    },
]

DRAWINGS = [
    {
        "id": "TQ2022-4332A",
        "label": "双线一字形桥台（桥通（2022）4332A-Ⅰ）",
        "familyTemplate": "abutment_family",
        "textTokens": [r"4332\s*A", "双线一字形桥台"],
        "filenameTokens": ["4332A", "双线一字形桥台"],
        "tableName": "台身尺寸及工程数量表",
        "drawingCode": "中国铁设桥通（2022）4332A-Ⅰ",
        "category": "BridgeSubstructure",
        "semanticCode": "Bridge.Substructure.StraightAbutment",
        "designSpeed": "350km/h",
        "applicableSpan": ["24m(高)简支箱梁", "32m简支箱梁"],
        "extractionMode": "canonical_rules_inferred",
        "templateTruth": True,
        "annotations": ABUTMENT_ANNOTATIONS,
    },
    {
        "id": "TQ2016-2322A",
        "label": "预制无砟轨道后张法预应力混凝土简支箱梁（双线）31.5m",
        "componentName": "预制箱梁",
        "familyTemplate": "boxgirder_family",
        "textTokens": [r"2322\s*A"],
        "filenameTokens": ["2322A"],
        "tableName": "每孔梁工程数量表（主梁 混凝土）",
        "sheet": "通桥（2016）2322A-Ⅱ-08 工程数量表",
        "drawingCode": "通桥（2016）2322A-Ⅱ-1",
        "category": "BridgeSuperstructure",
        "semanticCode": "Bridge.Superstructure.BoxGirder",
        "designSpeed": "350km/h",
        "applicableSpan": ["31.5m简支箱梁"],
        "extractionMode": "scan_vision_read",
        "templateTruth": True,
        "annotations": BOX_GIRDER_ANNOTATIONS,
        "notes": [
            "源 PDF 为 48 页 A0 扫描件（DocuWide6055，2016-07-19），无文字层，"
            "数量表按页读图录入并登记于模板族 table_truth.json。",
            "梁体截面轮廓取自同图号三维模型实测并归一化，倒角/承托以直线近似。",
        ],
    },
]


def match(text: str, filename: str):
    """按文字层优先、文件名兜底匹配登记表，返回 (drawing, basis) 或 (None, None)。"""
    body = str(text or "")
    if body.strip():
        for drawing in DRAWINGS:
            for token in drawing.get("textTokens") or []:
                if re.search(token, body, re.IGNORECASE):
                    return drawing, "text_layer"
    name = str(filename or "")
    for drawing in DRAWINGS:
        for token in drawing.get("filenameTokens") or []:
            if token and token.lower() in name.lower():
                return drawing, ("filename_no_text_layer" if not body.strip() else "filename")
    return None, None


def template_dir(drawing: dict) -> str:
    """登记表 → 模板族源码目录（须含 generators/generator.py）。"""
    roots = [
        os.environ.get("RCIF_TEMPLATES_DIR") or "",
        ROOT,
        HERE,
    ]
    candidates = []
    for root in roots:
        if not root:
            continue
        candidates.append(os.path.join(root, str(drawing.get("familyTemplate"))))
    for path in candidates:
        if os.path.isfile(os.path.join(path, "generators", "generator.py")):
            return path
    raise FileNotFoundError(
        f"未找到图号 {drawing.get('id')} 对应的模板族目录: {drawing.get('familyTemplate')}"
        f"（查找根: {[r for r in roots if r]}）"
    )


def describe(drawing: dict) -> dict:
    """给转换元数据用的只读描述。"""
    keys = (
        "id", "label", "componentName", "drawingCode", "category", "semanticCode",
        "designSpeed", "applicableSpan", "tableName", "sheet",
        "extractionMode", "notes",
    )
    return {k: drawing[k] for k in keys if k in drawing}
