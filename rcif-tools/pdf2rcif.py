"""PDF -> RCIF 参数化构件（草稿）转换器 (pdf2rcif.py)

流程：
1. 已登记图号（drawings.py）：解析对应模板族，走专用模板 + 已校核数量表；
2. 未登记图号：走 `pdf_generic.py` 通用解析（文字层/类型关键词/标注尺寸），
   生成可审阅的体量草稿，标注 `generic_heuristic_draft` 与置信度/缺失参数；
3. 模板路径复制 generator / geometry / display / component.json，通用路径直接生成；
4. 重写 identity / description / qa 后打包。

诚实边界：
- 已登记图号不做 OCR：纯扫描件按图号登记表匹配，数量表真值取自模板族登记值；
- 未登记图号没有专用模板：通用解析只做文字/尺寸启发式与体量，不做精确 2D→3D；
- 因此两条路径输出都是 **参数化草稿**，必须由工程师通过 3D 截图、脚本批注和
  多轮对话复核后才能送审。

用法：
  python pdf2rcif.py drawing.pdf out/BoxGirder_1.0.0.rcif --family-id xl315 --revision 1.0.0
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import shutil
import sys
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import drawings  # noqa: E402
import pack_rcif_cli  # noqa: E402
import pdf_ingest  # noqa: E402


def _copy_template(template_family: str, draft_dir: str) -> None:
    for source, target in [
        (os.path.join(template_family, "generators", "generator.py"),
         os.path.join(draft_dir, "generators", "generator.py")),
        (os.path.join(template_family, "rcif", "geometry"), os.path.join(draft_dir, "rcif", "geometry")),
        (os.path.join(template_family, "rcif", "display"), os.path.join(draft_dir, "rcif", "display")),
        (os.path.join(template_family, "rcif", "component.json"), os.path.join(draft_dir, "rcif", "component.json")),
    ]:
        if not os.path.exists(source):
            continue
        os.makedirs(os.path.dirname(target), exist_ok=True)
        if os.path.isdir(source):
            shutil.copytree(source, target, dirs_exist_ok=True)
        else:
            shutil.copy2(source, target)


def convert_pdf_to_rcif(
    pdf_path: str,
    output_rcif: str,
    family_id: str = "PdfDraft",
    revision: str = "0.1.0",
    name: str | None = None,
) -> tuple[str, dict]:
    pdf_path = os.path.abspath(pdf_path)
    output_rcif = os.path.abspath(output_rcif)
    if not os.path.isfile(pdf_path):
        raise FileNotFoundError(f"PDF 文件不存在: {pdf_path}")
    if not output_rcif.lower().endswith(".rcif"):
        output_rcif = os.path.join(output_rcif, f"{family_id}_{revision}.rcif")

    # 图号解析先做一次，便于在建立暂存目录前就给出明确的拒绝原因
    import pymupdf  # 局部导入，避免无 PDF 依赖环境下 import 失败

    with pymupdf.open(pdf_path) as doc:
        probe_text = ""
        for i in range(min(5, len(doc))):
            probe_text += doc[i].get_text("text") + "\n"
    drawing, basis = drawings.match(probe_text, os.path.basename(pdf_path))
    if drawing is None:
        # 未登记图号不再直接拒绝：走通用“文字/尺寸/类型 + 体量草稿”管线。
        # 草稿明确标记 generic_heuristic_draft，由工程师通过 3D/脚本/对话复核。
        from pdf_generic import convert_generic_pdf
        return convert_generic_pdf(
            pdf_path,
            output_rcif,
            family_id=family_id,
            revision=revision,
            name=name,
        )
    template_family = drawings.template_dir(drawing)

    # 构件名称优先级：显式 --name > 图号登记表默认名称 > PDF 文件名
    resolved_name = name or drawing.get("componentName") or os.path.splitext(os.path.basename(pdf_path))[0]

    stage_dir = os.path.join(os.path.dirname(output_rcif), f".pdf_stage_{uuid.uuid4().hex[:8]}")
    os.makedirs(stage_dir, exist_ok=True)
    try:
        ingest = pdf_ingest.ingest_pdf(pdf_path, stage_dir, template_dir=template_family)
        _copy_template(template_family, stage_dir)

        component_path = os.path.join(stage_dir, "rcif", "component.json")
        with open(component_path, "r", encoding="utf-8") as f:
            component = json.load(f)
        component.setdefault("identity", {})
        component["identity"].update({
            "componentId": str(uuid.uuid5(
                uuid.NAMESPACE_URL,
                f"rcif:pdf:{ingest.get('pdfSha256', '')}:{family_id}:{revision}",
            )),
            "familyId": family_id,
            "revision": revision,
            "name": resolved_name,
            "sourceMode": "parametric",
            "drawingCode": drawing.get("drawingCode"),
            "category": drawing.get("category"),
            "semanticCode": drawing.get("semanticCode"),
            "designSpeed": drawing.get("designSpeed"),
            "applicableSpan": drawing.get("applicableSpan"),
        })
        component["description"] = (
            f"由标准图 {os.path.basename(pdf_path)}（图号 {drawing.get('drawingCode')}）"
            f"摄取生成的参数化草稿；数量表真值取自该图号已校核登记值，"
            f"参数与截面须由工程师复核后方可用于生产。"
        )
        component["qa"] = {
            **(component.get("qa") or {}),
            "extractionMode": ingest.get("extractionMode", "canonical_rules_inferred"),
            "pdfFingerprint": ingest.get("fingerprint"),
            "fingerprintBasis": ingest.get("fingerprintBasis"),
            "sourceTextLayer": ingest.get("textLayer"),
            "pageCount": ingest.get("pageCount"),
            "sourcePdfSha256": ingest.get("pdfSha256"),
            "drawingCode": drawing.get("drawingCode"),
            "tableTruth": drawing.get("tableName"),
            "componentName": resolved_name,
            "componentNameSource": (
                "cli_argument" if name
                else ("drawing_registry" if drawing.get("componentName") else "source_filename")
            ),
            "sourceNotes": drawing.get("notes") or [],
        }
        component.setdefault("changelog", []).append({
            "version": revision,
            "date": datetime.date.today().isoformat(),
            "author": "RCIF-DSH-Studio",
            "type": "initial",
            "changes": [
                f"由 {os.path.basename(pdf_path)} 摄取生成参数化草稿",
                f"图号 {drawing.get('drawingCode')}，{ingest.get('pageCount', 0)} 页，"
                f"指纹依据 {ingest.get('fingerprintBasis')}",
                f"数量表真值：{drawing.get('tableName')}（{ingest.get('extractionMode')}）",
            ],
        })
        with open(component_path, "w", encoding="utf-8") as f:
            json.dump(component, f, ensure_ascii=False, indent=2)

        pkg_path, pkg_sha, provenance = pack_rcif_cli.pack_rcif_with_sandbox_and_provenance(
            stage_dir, output_rcif
        )
        return pkg_path, {
            "packageSha256": pkg_sha,
            "provenance": provenance,
            "extractionMode": ingest.get("extractionMode", "canonical_rules_inferred"),
            "fingerprint": ingest.get("fingerprint"),
            "fingerprintBasis": ingest.get("fingerprintBasis"),
            "drawingId": ingest.get("drawingId"),
            "pageCount": ingest.get("pageCount"),
        }
    finally:
        shutil.rmtree(stage_dir, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="PDF 标准图 -> RCIF 参数化草稿")
    parser.add_argument("pdf_path", help="输入 PDF")
    parser.add_argument("output", help="输出 .rcif 路径或目录")
    parser.add_argument("--family-id", default="PdfDraft")
    parser.add_argument("--revision", default="0.1.0")
    parser.add_argument("--name", default=None)
    args = parser.parse_args()
    try:
        pkg, info = convert_pdf_to_rcif(
            args.pdf_path, args.output, args.family_id, args.revision, args.name
        )
        print(f"[OK] 生成成功: {pkg} (SHA256={info['packageSha256'][:16]}..., mode={info['extractionMode']})")
        return 0
    except Exception as exc:  # noqa: BLE001 - CLI 边界
        print(f"[ERROR] PDF 转换失败: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
