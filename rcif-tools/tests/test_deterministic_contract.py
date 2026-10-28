"""RCIF v1.4.2 §5.1 确定性序列化跨语言契约测试。

固定向量由 `rcifc.det_json` 生成，同时被
`rcif-web/shared/deterministic.spec.ts` 读取，确保 TS 与 Python 输出逐字节一致。

运行: python -m unittest discover -s rcif-tools/tests -p "test_*.py"
"""

from __future__ import annotations

import hashlib
import io
import json
import math
import sys
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rcif-tools"))

import rcifc  # noqa: E402

try:  # 审批验签依赖 cryptography；缺失时只跳过该用例，其余契约测试仍可运行
    import pack_rcif_cli  # noqa: E402
except ModuleNotFoundError:  # pragma: no cover - 取决于运行环境
    pack_rcif_cli = None

FIXTURE = ROOT / "rcif-web" / "shared" / "fixtures" / "deterministic_vectors.json"
APPROVAL_FIXTURE = ROOT / "rcif-web" / "shared" / "fixtures" / "approval_canonical.json"


class DeterministicContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.fixture = json.loads(FIXTURE.read_text(encoding="utf-8"))

    def test_fixture_cases_are_byte_exact(self) -> None:
        self.assertGreater(len(self.fixture["cases"]), 10)
        for case in self.fixture["cases"]:
            with self.subTest(payload=case["payload"]):
                canonical = rcifc.det_json(case["payload"])
                self.assertEqual(canonical, case["canonical"])
                self.assertEqual(
                    hashlib.sha256(canonical.encode("utf-8")).hexdigest(),
                    case["sha256"],
                )

    def test_number_formatting_follows_c_percent_g(self) -> None:
        cases = {
            1.23456789: "1.23456789",          # 旧实现 %.6f 会截断为 1.234568
            0.0000015: "1.5e-06",              # 指数 < -4 用科学计数法
            0.000015: "1.5e-05",
            0.0001: "0.0001",
            1e20: "1e+20",                     # 旧实现会输出 20 位整数
            1234567890123456: "1.23456789012e+15",
            -0.0: "0",
        }
        for value, expected in cases.items():
            with self.subTest(value=value):
                self.assertEqual(rcifc.format_number(value), expected)

    def test_non_finite_rejected(self) -> None:
        for value in (math.nan, math.inf, -math.inf):
            with self.subTest(value=value), self.assertRaises(ValueError):
                rcifc.format_number(value)

    def test_spec_vectors_pass(self) -> None:
        vectors = rcifc.verify_vectors()
        self.assertEqual(len(vectors), 4)
        for vector in vectors:
            with self.subTest(vector=vector["vector"]):
                self.assertTrue(vector["ok"], vector)

    def test_package_is_reproducible(self) -> None:
        stage = ROOT / "dsh-rcif" / "workbench"  # 只读样例：打包不得修改源目录
        component = {
            "format": "cbf-component",
            "schemaVersion": "1.4.2",
            "identity": {"componentId": "11111111-1111-5111-8111-111111111111"},
        }
        first, first_digest = rcifc.build_rcif_archive(str(stage), component)
        second, second_digest = rcifc.build_rcif_archive(str(stage), component)
        self.assertEqual(first_digest, second_digest, "同一内容两次打包必须得到相同 SHA-256")
        self.assertEqual(first, second)
        archive = zipfile.ZipFile(io.BytesIO(first))
        names = archive.namelist()
        self.assertEqual(names, sorted(names), "归档条目必须按路径排序")
        self.assertIn("checksums.json", names)
        self.assertIn("component.json", names)
        checksums = json.loads(archive.read("checksums.json"))
        self.assertEqual(checksums["algorithm"], "sha256")
        self.assertIn("component.json", checksums["files"])

    def test_approval_canonical_payload_matches_host(self) -> None:
        fixture = json.loads(APPROVAL_FIXTURE.read_text(encoding="utf-8"))
        canonical = json.dumps(
            fixture["payload"],
            sort_keys=True,
            separators=(",", ":"),
            ensure_ascii=False,
        )
        self.assertEqual(canonical, fixture["canonical"])
        self.assertEqual(
            hashlib.sha256(canonical.encode("utf-8")).hexdigest(),
            fixture["sha256"],
        )

    def test_ed25519_approval_verification_round_trip(self) -> None:
        if pack_rcif_cli is None:
            self.skipTest("cryptography 未安装，跳过 Ed25519 验签用例")
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

        fixture = json.loads(APPROVAL_FIXTURE.read_text(encoding="utf-8"))
        private_key = Ed25519PrivateKey.generate()
        signature = private_key.sign(fixture["canonical"].encode("utf-8"))
        approval = {
            **fixture["payload"],
            "approvalAlgorithm": "Ed25519",
            "approvalSignature": pack_rcif_cli.base64url_encode(signature),
            "approvalVerified": True,
        }
        self.assertTrue(
            pack_rcif_cli.verify_ed25519_approval(approval, private_key.public_key(), "", "")
        )

        tampered = {**approval, "approvedBy": "mallory"}
        with self.assertRaises(Exception):
            pack_rcif_cli.verify_ed25519_approval(tampered, private_key.public_key(), "", "")


if __name__ == "__main__":
    unittest.main()
