"""RCIF 通用构件编译器与打包工具 (rcifc.py)
遵循 RCIF v1.4.2 规范 (rcif_component_format_spec_v1.4.2.md)

功能：
- 严格按照 RCIF v1.4.2 §5.1 计算确定性 cacheKey 并通过 4 条标准测试向量自检
- 支持参数化构件 (sourceMode: 'parametric') 与原厂精确实体构件 (sourceMode: 'native-brep')
- 自动生成符合 Schema 的 component.json、checksums.json (统一使用 {"algorithm":"sha256","files":{...}} 规范结构)
- 打包产出规范化的 .rcif 构件包
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
import os
import sys
import zipfile
from typing import Any, Dict, List, Tuple


def format_number(val: float) -> str:
    """浮点数按 RCIF v1.4.2 §5.1 的 C `%.12g` 规范格式化。

    - `%.12g` 语义：指数 < -4 或 >= 12 时用科学计数法（指数两位补零），
      否则用定点小数，并去掉多余的尾零。
    - 禁止 NaN / Inf，`-0.0` 归一为 `0`。
    """
    if isinstance(val, bool):  # 防御：bool 是 int 子类
        return "true" if val else "false"
    if not math.isfinite(val):
        raise ValueError(f"DeterministicJSON: 不允许 NaN/Inf ({val!r})")
    # 规范 §5.1：|val| < 1e-12 视为 0（与 TS formatNumber 保持一致）
    if val == 0 or abs(val) < 1e-12:
        return "0"
    text = format(val, ".12g")
    return "0" if text == "-0" else text


def det_json(value: Any) -> str:
    """RCIF v1.4.2 §5.1 DeterministicJSON 严格规范序列化。"""
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return format_number(value)
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False)
    if isinstance(value, (list, tuple)):
        return "[" + ",".join(det_json(v) for v in value) + "]"
    if isinstance(value, dict):
        keys = sorted(value.keys(), key=lambda k: k.encode("utf-8"))
        return "{" + ",".join(
            f"{json.dumps(k, ensure_ascii=False)}:{det_json(value[k])}" for k in keys
        ) + "}"
    raise TypeError(f"DeterministicJSON: 不支持的类型 {type(value)}")


def sha256_hex(data: Any) -> str:
    if isinstance(data, str):
        data = data.encode("utf-8")
    return hashlib.sha256(data).hexdigest()


def compute_cache_key(
    digest: str,
    inputs: Dict[str, Any],
    source: str = "generators/generator.py",
    engine: str = "cadquery",
    version: str = "1.4.2",
    kernel_id: str = "occt",
    kernel_version: str = "8.0.0",
) -> str:
    payload = {
        "cacheKeyVersion": 1,
        "digest": digest,
        "engine": engine,
        "inputParams": inputs,
        "kernelId": kernel_id,
        "kernelVersion": kernel_version,
        "source": source,
        "tolerance": {"angular": 0.001, "linear": 0.01},
        "units": {"angle": "rad", "length": "mm"},
        "version": version,
    }
    return sha256_hex(det_json(payload))


def verify_vectors() -> List[Dict[str, Any]]:
    """规范 §5.1 四条标准测试向量自检。"""
    vectors = [
        (
            {"a": 1, "b": 1.5, "c": None, "d": [10, 20]},
            "51a01f79a9ca9470a9d4373b8f06ab39e92a849bed2035ee58d92f7bc640cb0c",
        ),
        (
            {"Layout": "梅花形"},
            "1135ce65b3a41eb7ab8e979d812eb41d730b8e0227d76c9dac782cb32497d647",
        ),
        (
            {
                "cacheKeyVersion": 1,
                "digest": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                "engine": "cadquery",
                "inputParams": {
                    "Alignment": None,
                    "GroundSurface": None,
                    "Height": 8000,
                    "Length": 20000,
                    "SlopeRatio": 1.5,
                    "Tiers": [],
                },
                "kernelId": "occt",
                "kernelVersion": "8.0.0",
                "source": "service://subgrade_slope",
                "tolerance": {"angular": 0.001, "linear": 0.01},
                "units": {"angle": "rad", "length": "mm"},
                "version": "1.0.0",
            },
            "0fee26374b7a518d8098da4ad6c1ad52716d9f33f5bf183fe0f75f90f4bafac0",
        ),
        (
            {
                "cacheKeyVersion": 1,
                "digest": "0c034d0fe7643a9ffcfffb1d79ec9ce2363a0e79baa0ca599cb99cd259f1eb0d",
                "engine": "build123d",
                "inputParams": {
                    "CrossSlope": 2,
                    "PierHeight": 12000,
                    "Slope": 35,
                    "TopLength": 6400,
                    "TopWidth": 2400,
                },
                "kernelId": "occt",
                "kernelVersion": "8.0.0",
                "source": "generators/generator.py",
                "tolerance": {"angular": 0.001, "linear": 0.01},
                "units": {"angle": "rad", "length": "mm"},
                "version": "1.4.2",
            },
            "8361c4b4b7916b48c16a2687f3d73f1fb3ccf1f850f12e201333f77257697405",
        ),
    ]
    out = []
    for i, (payload, expected) in enumerate(vectors, 1):
        actual = sha256_hex(det_json(payload))
        out.append({"vector": i, "ok": actual == expected, "actual": actual, "expected": expected})
    return out


def build_rcif_archive(
    stage_dir: str,
    component_json_dict: Dict[str, Any],
) -> Tuple[bytes, str]:
    """在内存中构建 .rcif 归档（不修改 stage_dir），返回 (archive_bytes, sha256)。

    确定性保证：
    - 遍历顺序固定（目录/文件均排序）；
    - 所有 ZipInfo 使用固定时间戳 (1980-01-01) 与固定权限位；
    - component.json 与 checksums.json 由本函数统一生成。
    """
    entries: List[Tuple[str, bytes]] = []
    files_map: Dict[str, str] = {}

    for root, dirs, files in os.walk(stage_dir):
        dirs.sort()
        for file in sorted(files):
            if file in ("checksums.json",) or (root == stage_dir and file == "component.json"):
                continue
            fpath = os.path.join(root, file)
            relpath = os.path.relpath(fpath, stage_dir).replace("\\", "/")
            with open(fpath, "rb") as rf:
                data = rf.read()
            entries.append((relpath, data))
            files_map[relpath] = sha256_hex(data)

    component_bytes = json.dumps(component_json_dict, ensure_ascii=False, indent=2).encode("utf-8")
    entries.append(("component.json", component_bytes))
    files_map["component.json"] = sha256_hex(component_bytes)

    checksums_doc = {"algorithm": "sha256", "files": files_map}
    checksums_bytes = json.dumps(checksums_doc, indent=2).encode("utf-8")
    entries.append(("checksums.json", checksums_bytes))

    entries.sort(key=lambda item: item[0])

    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for relpath, data in entries:
            info = zipfile.ZipInfo(relpath, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            zf.writestr(info, data)
    archive = buffer.getvalue()
    return archive, sha256_hex(archive)


def build_rcif_package(
    stage_dir: str,
    output_rcif_path: str,
    component_json_dict: Dict[str, Any],
) -> Tuple[str, str]:
    """将构件目录打包为 .rcif 规范归档包，并返回 (output_path, package_sha256)。"""
    os.makedirs(os.path.dirname(os.path.abspath(output_rcif_path)), exist_ok=True)
    archive, digest = build_rcif_archive(stage_dir, component_json_dict)
    with open(output_rcif_path, "wb") as f:
        f.write(archive)
    return output_rcif_path, digest


def main():
    parser = argparse.ArgumentParser(description="RCIF 通用构件编译器与向量自检工具 (rcifc.py)")
    parser.add_argument("--verify", action="store_true", help="执行规范 §5.1 标准测试向量自检")
    parser.add_argument("--stage", type=str, help="待打包的构件临时目录")
    parser.add_argument("--out", type=str, help="输出 .rcif 构件包路径")
    args = parser.parse_args()

    v_results = verify_vectors()
    all_ok = all(v["ok"] for v in v_results)
    print(f"RCIF v1.4.2 §5.1 Standard Vectors Self-Test: {'ALL PASSED' if all_ok else 'FAILED'}")
    for v in v_results:
        print(f"  Vector {v['vector']}: {'OK' if v['ok'] else 'FAIL'}")

    if args.stage and args.out:
        comp_path = os.path.join(args.stage, "component.json")
        if not os.path.exists(comp_path):
            print(f"错误: 目录 {args.stage} 下未找到 component.json", file=sys.stderr)
            sys.exit(1)
        with open(comp_path, "r", encoding="utf-8") as f:
            comp_data = json.load(f)
        pkg, sha = build_rcif_package(args.stage, args.out, comp_data)
        print(f"[OK] 构件包已生成: {pkg} (SHA256: {sha[:16]}...)")

    if not all_ok:
        sys.exit(1)


if __name__ == "__main__":
    main()
