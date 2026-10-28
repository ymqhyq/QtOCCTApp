"""沙箱内安全打包器与审批验签 CLI (pack_rcif_cli.py)
遵循方案第一部分 §4.3、§5.3 与第三部分 1 (S4, S4-N1, S5) 契约

核心能力：
- 接收 Host 签发的 Ed25519 审批单并使用只读公钥 HOST_APPROVAL_PUBKEY 验签
- 比对 approvedTableTruthSha256 == sha256(table_truth.json)
- 在内存现场重新执行 verify_sandbox.run_all_checks_in_memory()
- 生成符合 rcif-provenance/1 规范的 audit_provenance.json
- 打包产出正式 .rcif 构件包
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import shutil
import sys
import tempfile
import uuid
from typing import Any, Dict, Optional, Tuple

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import rcifc
import verify_sandbox


def base64url_decode(s: str) -> bytes:
    """标准 base64url 无填充解码。"""
    s = s.replace("-", "+").replace("_", "/")
    pad = (4 - len(s) % 4) % 4
    s += "=" * pad
    return base64.b64decode(s)


def base64url_encode(b: bytes) -> str:
    """标准 base64url 无填充编码。"""
    return base64.b64encode(b).decode("ascii").rstrip("=").replace("+", "-").replace("/", "_")


def sha256_file(path: str) -> str:
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def verify_ed25519_approval(
    approval_data: Dict[str, Any],
    pubkey: Ed25519PublicKey,
    actual_table_truth_sha256: str,
    actual_params_sha256: str,
) -> bool:
    """使用公钥核验审批单数字签名与文件摘要。"""
    signature_b64 = approval_data.get("approvalSignature")
    if not signature_b64:
        raise ValueError("审批单缺失 approvalSignature 字段")

    # 比对文件哈希
    if actual_table_truth_sha256 and approval_data.get("approvedTableTruthSha256") != actual_table_truth_sha256:
        raise ValueError(
            f"table_truth.json 哈希不匹配！审批值: {approval_data.get('approvedTableTruthSha256')}, 实测值: {actual_table_truth_sha256}"
        )
    if actual_params_sha256 and approval_data.get("approvedParamsSha256") != actual_params_sha256:
        raise ValueError(
            f"params.csv 哈希不匹配！审批值: {approval_data.get('approvedParamsSha256')}, 实测值: {actual_params_sha256}"
        )

    # 规范化待签数据 payload (与 Host 签名端严格一致)
    sign_payload = {
        "approvalId": approval_data.get("approvalId"),
        "sessionId": approval_data.get("sessionId"),
        "approvedParamsSha256": approval_data.get("approvedParamsSha256"),
        "approvedTableTruthSha256": approval_data.get("approvedTableTruthSha256"),
        "approvedSourceMode": approval_data.get("approvedSourceMode"),
        "approvedExactBrep": approval_data.get("approvedExactBrep"),
        "approvedBy": approval_data.get("approvedBy"),
        "signedAt": approval_data.get("signedAt"),
    }
    # Must match the Host signer byte-for-byte: sorted keys, compact separators,
    # and UTF-8 raw output (ensure_ascii=False).
    payload_bytes = json.dumps(
        sign_payload, sort_keys=True, separators=(",", ":"), ensure_ascii=False
    ).encode("utf-8")
    sig_bytes = base64url_decode(signature_b64)

    pubkey.verify(sig_bytes, payload_bytes)
    return True


def pack_rcif_with_sandbox_and_provenance(
    family_dir: str,
    output_rcif_path: str,
    pubkey_bytes: Optional[bytes] = None,
    approval_json_path: Optional[str] = None,
    operator_user_id: str = "u_bridge_01",
    tenant_id: str = "t_crdc",
    session_id: str = "session-rcif-authoring-default",
) -> Tuple[str, str, Dict[str, Any]]:
    """在沙箱受限进程内执行门禁现场重跑，并在通过后打包 .rcif 与生成审计溯源数据。"""
    # 1. 现场重跑全套门禁
    report = verify_sandbox.run_all_checks_in_memory(family_dir)
    if report.blocker_count > 0 or report.major_count > 0:
        raise RuntimeError(
            f"[GATE_LOCK_REJECTED] 现场门禁未通过！Blocker={report.blocker_count}, Major={report.major_count}"
        )

    # 2. 如果提供了审批单与公钥，进行 Ed25519 验签 (fail-closed, ISSUE-011)
    approval_info = None
    if approval_json_path is not None:
        if not os.path.isfile(approval_json_path):
            raise FileNotFoundError(f"指定的审批单不存在: {approval_json_path}")
        if not pubkey_bytes:
            raise ValueError("已指定审批单但缺少公钥，按安全规程拒绝在未验签状态下放行打包 (fail-closed)")
        with open(approval_json_path, "r", encoding="utf-8") as f:
            approval_data = json.load(f)
        pubkey = Ed25519PublicKey.from_public_bytes(pubkey_bytes)
        verify_ed25519_approval(
            approval_data,
            pubkey,
            report.table_truth_sha256,
            report.params_sha256,
        )
        approval_data["approvalVerified"] = True
        approval_info = approval_data

    # 3. 准备打包隔离舞台目录并保证事后可靠清理 (ISSUE-006 & ISSUE-009)
    out_abs = os.path.abspath(output_rcif_path)
    stage_dir = os.path.join(os.path.dirname(out_abs), f".rcif_stage_{uuid.uuid4().hex[:8]}")
    os.makedirs(stage_dir, exist_ok=True)
    try:
        # 读取已有 component.json 或生成初稿
        comp_json_path = os.path.join(family_dir, "rcif", "component.json")
        if not os.path.isfile(comp_json_path):
            comp_json_path = os.path.join(family_dir, "component.json")

        if os.path.isfile(comp_json_path):
            with open(comp_json_path, "r", encoding="utf-8") as f:
                comp_dict = json.load(f)
        else:
            comp_dict = {
                "format": "cbf-component",
                "schemaVersion": "1.4.2",
                "identity": {
                    "componentId": str(uuid.uuid4()),
                    "familyId": os.path.basename(family_dir),
                    "revision": "1.0.0",
                    "name": os.path.basename(family_dir),
                    "sourceMode": "parametric",
                },
                "geometry": {"exactBrep": True},
                "recipe": {"source": "generators/generator.py"},
            }

        # 复制 geometry / display / generators，使包内自带可审阅的生成器源码
        for sub in ["geometry", "display", "generators"]:
            src_sub = os.path.join(family_dir, "rcif", sub)
            if not os.path.exists(src_sub):
                src_sub = os.path.join(family_dir, sub)
            if os.path.exists(src_sub):
                dst_sub = os.path.join(stage_dir, sub)
                shutil.copytree(src_sub, dst_sub, dirs_exist_ok=True)

        # 4. 打包 .rcif
        pkg_path, pkg_sha256 = rcifc.build_rcif_package(stage_dir, output_rcif_path, comp_dict)

        # 5. 生成 audit_provenance.json (如实记录隔离模式，ISSUE-002)
        provenance = {
            "schema": "rcif-provenance/1",
            "dshVersion": "0.2.1-rc.3",
            "sessionId": session_id,
            "operatorUserId": operator_user_id,
            "tenantId": tenant_id,
            "modelProvider": "deepseek-official",
            "modelsUsed": ["deepseek-chat", "deepseek-reasoner"],
            "sourceInputs": [],
            "humanConfirmations": [approval_info] if approval_info else [],
            "sandboxIsolation": "multiprocessing-spawn:safe-builtins:timeout-30s",
            "toolHashes": {
                "verify_sandbox.py": sha256_file(os.path.join(HERE, "verify_sandbox.py")),
                "pack_rcif_cli.py": sha256_file(os.path.join(HERE, "pack_rcif_cli.py")),
                "rcifc.py": sha256_file(os.path.join(HERE, "rcifc.py")),
            },
            "packageDigest": pkg_sha256,
            "signatureStatus": "verified" if approval_info else "unsigned-dev-build",
        }

        # 保存 provenance 文件在同一输出目录 (大小写不敏感的后缀替换)
        prov_path = os.path.splitext(output_rcif_path)[0] + ".provenance.json"
        with open(prov_path, "w", encoding="utf-8") as f:
            json.dump(provenance, f, ensure_ascii=False, indent=2)

        return pkg_path, pkg_sha256, provenance
    finally:
        shutil.rmtree(stage_dir, ignore_errors=True)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="RCIF 沙箱打包与审批验签 CLI (pack_rcif_cli.py)")
    parser.add_argument("family_dir", nargs="?", default="abutment_family", help="构件族源码目录")
    parser.add_argument(
        "output",
        nargs="?",
        default="rcif-tools/out/case1/StraightAbutment_1.0.0.rcif",
        help="输出 .rcif 路径",
    )
    parser.add_argument("--approval", help="Host 签发的审批单 JSON 路径")
    parser.add_argument("--pubkey", help="Host Ed25519 公钥原始字节的十六进制 (32 字节)")
    parser.add_argument("--session", default="session-rcif-authoring-default", help="会话 ID")
    parser.add_argument("--operator", default="u_bridge_01", help="操作者用户 ID")
    parser.add_argument("--tenant", default="t_crdc", help="租户 ID")
    args = parser.parse_args(argv)

    pubkey_bytes = None
    if args.pubkey:
        try:
            pubkey_bytes = bytes.fromhex(args.pubkey)
        except ValueError:
            print("错误: --pubkey 不是合法十六进制", file=sys.stderr)
            return 2
        if len(pubkey_bytes) != 32:
            print("错误: --pubkey 必须是 32 字节 Ed25519 公钥", file=sys.stderr)
            return 2
    if args.approval and not pubkey_bytes:
        print("错误: 指定 --approval 时必须同时提供 --pubkey (fail-closed)", file=sys.stderr)
        return 2

    try:
        pkg, sha, prov = pack_rcif_with_sandbox_and_provenance(
            args.family_dir,
            args.output,
            pubkey_bytes=pubkey_bytes,
            approval_json_path=args.approval,
            operator_user_id=args.operator,
            tenant_id=args.tenant,
            session_id=args.session,
        )
    except Exception as exc:  # noqa: BLE001 - CLI 边界必须转为非零退出码
        print(f"[GATE_LOCK_REJECTED] {exc}", file=sys.stderr)
        return 1

    print(f"[PACKAGED] {pkg}")
    print(f"  Package SHA-256: {sha}")
    print(f"  Signature Status: {prov['signatureStatus']}")
    print(f"  Tool Hashes: {prov['toolHashes']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

