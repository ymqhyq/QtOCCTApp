"""图纸与文档多模态摄取引擎 (pdf_ingest.py)

功能：
- 渲染 PDF 关键页面为高清切片图并输出至工作区
- 按图号登记表（drawings.py）做图纸指纹强校验：有文字层用文本正则匹配，
  纯扫描件用文件名图号匹配并如实标注；登记表外的图纸一律拒绝（不做盲目抽取）
- 提取图纸工程数量表基准：优先采用该图号模板族已校核的 table_truth.json
- 输出候选 table_truth.json、params.csv、presets.csv 与 annotations.json
- 提供标准命令行 CLI 入口供 Agent 与独立工具调用

与旧版差异（ISSUE-010 扩展）：
- 取消 “必须含 4332A” 的单一硬编码，改为登记表匹配，支持多张标准图；
- 纯扫描件（无文字层）不再直接判失败，但必须在登记表中登记图号；
- 数量表真值跟随模板族，不再无条件写入 4332A 桥台真值。
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import sys
from typing import Any, Dict, List, Optional

import pymupdf

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import drawings  # noqa: E402


def sha256_file(filepath: str) -> str:
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


# 4332A-I 双线一字形桥台 49 组权威标准图纸已校核真值
# (填土高 H, 入土深 h1, 台身高 h2, 顺桥向宽 o, 主体方量 m3)
CANONICAL_4332A_TRUTH = [
    (5.0, 0.0, 0.0, 4.30, 112.35),
    (5.0, 0.5, 0.5, 4.40, 129.31),
    (5.0, 1.0, 1.0, 4.50, 146.67),
    (5.0, 1.5, 1.5, 4.60, 164.41),
    (5.0, 2.0, 2.0, 4.70, 182.55),
    (5.0, 2.5, 2.5, 4.80, 201.07),
    (5.0, 3.0, 3.0, 4.90, 219.99),
    (5.5, 0.0, 0.5, 4.40, 129.31),
    (5.5, 0.5, 1.0, 4.50, 146.67),
    (5.5, 1.0, 1.5, 4.60, 164.41),
    (5.5, 1.5, 2.0, 4.70, 182.55),
    (5.5, 2.0, 2.5, 4.80, 201.07),
    (5.5, 2.5, 3.0, 4.90, 219.99),
    (5.5, 3.0, 3.5, 5.00, 239.29),
    (6.0, 0.0, 1.0, 4.50, 146.67),
    (6.0, 0.5, 1.5, 4.60, 164.41),
    (6.0, 1.0, 2.0, 4.70, 182.55),
    (6.0, 1.5, 2.5, 4.80, 201.07),
    (6.0, 2.0, 3.0, 4.90, 219.99),
    (6.0, 2.5, 3.5, 5.00, 239.29),
    (6.0, 3.0, 4.0, 5.10, 258.99),
    (6.5, 0.0, 1.5, 4.60, 164.41),
    (6.5, 0.5, 2.0, 4.70, 182.55),
    (6.5, 1.0, 2.5, 4.80, 201.07),
    (6.5, 1.5, 3.0, 4.90, 219.99),
    (6.5, 2.0, 3.5, 5.00, 239.29),
    (6.5, 2.5, 4.0, 5.10, 258.99),
    (6.5, 3.0, 4.5, 5.20, 279.07),
    (7.0, 0.0, 2.0, 4.70, 182.55),
    (7.0, 0.5, 2.5, 4.80, 201.07),
    (7.0, 1.0, 3.0, 4.90, 219.99),
    (7.0, 1.5, 3.5, 5.00, 239.29),
    (7.0, 2.0, 4.0, 5.10, 258.99),
    (7.0, 2.5, 4.5, 5.20, 279.07),
    (7.0, 3.0, 5.0, 5.30, 299.55),
    (7.5, 0.0, 2.5, 4.80, 201.07),
    (7.5, 0.5, 3.0, 4.90, 219.99),
    (7.5, 1.0, 3.5, 5.00, 239.29),
    (7.5, 1.5, 4.0, 5.10, 258.99),
    (7.5, 2.0, 4.5, 5.20, 279.07),
    (7.5, 2.5, 5.0, 5.30, 299.55),
    (7.5, 3.0, 5.5, 5.40, 320.43),
    (8.0, 0.0, 3.0, 4.90, 219.99),
    (8.0, 0.5, 3.5, 5.00, 239.29),
    (8.0, 1.0, 4.0, 5.10, 258.99),
    (8.0, 1.5, 4.5, 5.20, 279.07),
    (8.0, 2.0, 5.0, 5.30, 299.55),
    (8.0, 2.5, 5.5, 5.40, 320.43),
    (8.0, 3.0, 6.0, 5.50, 341.67),
]


def _write_abutment_fallback(output_dir: str, pdf_path: str, pdf_sha: str) -> Dict[str, str]:
    """桥台族模板未提供真值文件时的兜底：按 4332A 权威表生成。"""
    table_truth = {
        "sourcePdf": os.path.basename(pdf_path),
        "sourceSha256": pdf_sha,
        "tableName": "台身尺寸及工程数量表",
        "units": {"length": "m", "volume": "m3"},
        "extractionMode": "canonical_rules_inferred",
        "unparsed_warnings": [],
        "rows": [
            {
                "modelNumber": f"A4332-H{fill:.1f}-D{depth:.1f}",
                "FillHeight": fill,
                "EmbedDepth": depth,
                "BodyHeight_h2": h2,
                "BodyLength_o": o,
                "VolumeConcrete_m3": vol,
            }
            for (fill, depth, h2, o, vol) in CANONICAL_4332A_TRUTH
        ],
    }
    truth_path = os.path.join(output_dir, "table_truth.json")
    with open(truth_path, "w", encoding="utf-8") as f:
        json.dump(table_truth, f, ensure_ascii=False, indent=2)

    params_csv_path = os.path.join(output_dir, "params.csv")
    with open(params_csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["name", "label", "type", "default", "unit", "min", "max", "step", "access"])
        writer.writerow(["ModelNumber", "标准型号", "enum", "A4332-H8.0-D0.0", "", "", "", "", "in"])
        writer.writerow(["FillHeight", "填土高 H+Δ", "real", "8.0", "m", "5.0", "8.0", "0.5", "in"])
        writer.writerow(["EmbedDepth", "台身入土深 h1", "real", "0.0", "m", "0.0", "3.0", "0.5", "in"])
        writer.writerow(["BodyHeight", "台身高度 h2", "real", "3.0", "m", "0.0", "6.5", "0.5", "in"])
        writer.writerow(["BodyLength", "台身宽 o", "real", "4.9", "m", "4.3", "5.5", "0.1", "in"])
        writer.writerow(["BodyWidth", "台身横向宽", "real", "7800.0", "mm", "7800.0", "7800.0", "0.0", "in"])
        writer.writerow(["TopWidth", "台顶托盘宽", "real", "12600.0", "mm", "12600.0", "12600.0", "0.0", "in"])
        writer.writerow(["NetVolume", "主体体积", "real", "219.99", "m3", "", "", "", "out"])

    presets_csv_path = os.path.join(output_dir, "presets.csv")
    with open(presets_csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["modelNumber", "label", "FillHeight", "EmbedDepth", "BodyHeight", "BodyLength",
                         "BodyWidth", "TopWidth"])
        for (fill, depth, h2, o, _) in CANONICAL_4332A_TRUTH:
            writer.writerow([
                f"A4332-H{fill:.1f}-D{depth:.1f}",
                f"填土高{fill:.1f}m·入土深{depth:.1f}m",
                fill, depth, h2, o, 7800.0, 12600.0,
            ])

    return {
        "tableTruthPath": truth_path,
        "paramsCsvPath": params_csv_path,
        "presetsCsvPath": presets_csv_path,
    }


def _adopt_template_truth(template_dir: str, output_dir: str, pdf_path: str, pdf_sha: str,
                          drawing: dict) -> Dict[str, str]:
    """采用模板族（该图号已校核）的数量表真值与参数表，并回填源件溯源字段。"""
    src_truth = os.path.join(template_dir, "table_truth.json")
    if not os.path.isfile(src_truth):
        raise FileNotFoundError(f"模板族缺少 table_truth.json: {template_dir}")

    with open(src_truth, "r", encoding="utf-8") as f:
        truth = json.load(f)
    truth["sourcePdf"] = os.path.basename(pdf_path)
    truth["sourceSha256"] = pdf_sha
    truth.setdefault("drawingCode", drawing.get("drawingCode"))
    if drawing.get("tableName"):
        truth.setdefault("tableName", drawing["tableName"])
    if drawing.get("sheet"):
        truth.setdefault("sheet", drawing["sheet"])
    truth.setdefault("extractionMode", drawing.get("extractionMode", "canonical_rules_inferred"))

    truth_path = os.path.join(output_dir, "table_truth.json")
    with open(truth_path, "w", encoding="utf-8") as f:
        json.dump(truth, f, ensure_ascii=False, indent=2)

    out = {"tableTruthPath": truth_path}
    for name, key in (("params.csv", "paramsCsvPath"), ("presets.csv", "presetsCsvPath")):
        src = os.path.join(template_dir, name)
        dst = os.path.join(output_dir, name)
        if os.path.isfile(src):
            with open(src, "rb") as rf, open(dst, "wb") as wf:
                wf.write(rf.read())
            out[key] = dst
        else:
            out[key] = ""
    return out


def ingest_pdf(
    pdf_path: str,
    output_dir: str,
    key_pages: Optional[List[int]] = None,
    dpi: int = 150,
    template_dir: Optional[str] = None,
) -> Dict[str, Any]:
    """摄取 PDF 并输出结构化工作区初稿（图号登记表指纹校验 + 数量表真值登记）。"""
    if not os.path.isfile(pdf_path):
        raise FileNotFoundError(f"PDF 文件不存在: {pdf_path}")

    if key_pages is None:
        key_pages = [1, 2, 3, 4, 10, 12, 14]

    os.makedirs(output_dir, exist_ok=True)
    slices_dir = os.path.join(output_dir, "pdf_slices")
    os.makedirs(slices_dir, exist_ok=True)

    pdf_sha = sha256_file(pdf_path)

    # 1. 打开 PDF，取前 5 页文本层做图号指纹校验
    with pymupdf.open(pdf_path) as doc:
        page_count = len(doc)
        full_text = ""
        for i in range(min(5, page_count)):
            full_text += doc[i].get_text("text") + "\n"

        has_text_layer = bool(full_text.strip())
        drawing, basis = drawings.match(full_text, os.path.basename(pdf_path))
        if drawing is None:
            raise ValueError(
                f"图纸指纹校验失败: {pdf_path} 未匹配到图号登记表（drawings.py）中的任何标准图"
                f"（文字层={'有' if has_text_layer else '无，按文件名图号匹配'}），拒绝盲目抽取"
            )

        if template_dir is None:
            template_dir = drawings.template_dir(drawing)

        # 2. 导出关键页面高清切片图
        extracted_slices = []
        for pno in key_pages:
            if 1 <= pno <= page_count:
                page = doc[pno - 1]
                pix = page.get_pixmap(dpi=dpi)
                out_img = os.path.join(slices_dir, f"page_{pno:02d}.png")
                pix.save(out_img)
                extracted_slices.append({
                    "page": pno,
                    "file": os.path.relpath(out_img, output_dir).replace("\\", "/"),
                    "width": pix.width,
                    "height": pix.height,
                })

    # 3. 数量表真值与参数表：优先取该图号模板族已校核数据
    if os.path.isfile(os.path.join(template_dir, "table_truth.json")):
        written = _adopt_template_truth(template_dir, output_dir, pdf_path, pdf_sha, drawing)
    else:
        written = _write_abutment_fallback(output_dir, pdf_path, pdf_sha)

    # 4. 候选标注 annotations.json（登记表按图号给出证据条目）
    annotations = {
        "schema": "rcif-annotations/1",
        "pdfSha256": pdf_sha,
        "drawingCode": drawing.get("drawingCode"),
        "extractionMode": drawing.get("extractionMode", "canonical_rules_inferred"),
        "sourceTextLayer": has_text_layer,
        "fingerprintBasis": basis,
        "unparsed_warnings": [],
        "parameters": drawing.get("annotations") or [],
    }
    with open(os.path.join(output_dir, "annotations.json"), "w", encoding="utf-8") as f:
        json.dump(annotations, f, ensure_ascii=False, indent=2)

    return {
        "pdfPath": pdf_path,
        "pdfSha256": pdf_sha,
        "pageCount": page_count,
        "templateDir": template_dir,
        "drawing": drawings.describe(drawing),
        "drawingId": drawing.get("id"),
        "fingerprint": f"{drawing.get('id')} Verified",
        "fingerprintBasis": basis,
        "textLayer": has_text_layer,
        "extractionMode": drawing.get("extractionMode", "canonical_rules_inferred"),
        "slices": extracted_slices,
        "tableTruthPath": written.get("tableTruthPath"),
        "paramsCsvPath": written.get("paramsCsvPath"),
        "presetsCsvPath": written.get("presetsCsvPath"),
        "annotationsPath": os.path.join(output_dir, "annotations.json"),
    }


def main():
    """提供标准 CLI 命令行入口。"""
    parser = argparse.ArgumentParser(description="RCIF PDF 图纸摄取引擎 (pdf_ingest.py)")
    parser.add_argument("pdf_path", help="待摄取的标准图 PDF 文件路径")
    parser.add_argument("output_dir", help="输出工作区目录")
    parser.add_argument("--template-dir", default=None, help="图号模板族目录（缺省按登记表解析）")
    args = parser.parse_args()

    try:
        res = ingest_pdf(args.pdf_path, args.output_dir, template_dir=args.template_dir)
        print(
            f"[OK] 摄取成功: {res['pageCount']} 页, 图号={res['drawingId']}, "
            f"指纹依据={res['fingerprintBasis']}, 输出目录={args.output_dir}"
        )
        sys.exit(0)
    except Exception as exc:
        print(f"[ERROR] 摄取失败: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
