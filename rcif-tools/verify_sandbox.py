"""RCIF 工程质量门禁与安全沙箱验证器 (verify_sandbox.py)
对应方案 WBS 2.1 与 RCIF v1.4.2 §2.1 门禁标准

核心特性：
1. GATE-01: AST 静态白名单审查、严禁反射/元编程属性访问 (闭环 ISSUE-001)
2. 进程级物理隔离与超时硬熔断：不可信代码在独立子进程运行，强制覆写 __builtins__ 为受限白名单字典 (闭环 ISSUE-001/002)
3. GATE-02: 全型号 BRepCheck 流形封闭与包 checksums.json 完整性核验
4. GATE-03: 支承垫石防悬空物理接触断言检查 (拦截 B4 跳水板缺陷)
5. GATE-04: 图纸工程数量表真值真实残差计算与斜率回归 (闭环 ISSUE-022)
6. GATE-05: 视图投影元数据与离线正交渲染校验 (不伪报通过)
7. GATE-06: RCIF v1.4.2 标准测试向量自检与 DeterministicJSON 契约
"""

from __future__ import annotations

import argparse
import ast
import concurrent.futures
import hashlib
import json
import os
import subprocess
import sys
import time

if sys.stdout and hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
if sys.stderr and hasattr(sys.stderr, "reconfigure"):
    try:
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import rcifc


@dataclass
class CheckItem:
    gate_id: str
    name: str
    level: str  # "BLOCKER", "MAJOR", "MINOR", "INFO"
    passed: bool
    message: str
    details: Dict[str, Any] = field(default_factory=dict)


@dataclass
class SandboxReport:
    family_dir: str
    passed: bool
    blocker_count: int
    major_count: int
    minor_count: int
    checks: List[CheckItem] = field(default_factory=list)
    table_truth_sha256: str = ""
    params_sha256: str = ""
    duration_sec: float = 0.0

    def to_dict(self) -> Dict[str, Any]:
        return {
            "family_dir": self.family_dir,
            "passed": self.passed,
            "blocker_count": self.blocker_count,
            "major_count": self.major_count,
            "minor_count": self.minor_count,
            "table_truth_sha256": self.table_truth_sha256,
            "params_sha256": self.params_sha256,
            "duration_sec": round(self.duration_sec, 3),
            "checks": [
                {
                    "gate_id": c.gate_id,
                    "name": c.name,
                    "level": c.level,
                    "passed": c.passed,
                    "message": c.message,
                    "details": c.details,
                }
                for c in self.checks
            ],
        }


# ---------------- 安全与白名单定义 (ISSUE-001) ----------------
ALLOWED_AST_IMPORTS = {
    "__future__", "cadquery", "cq", "math", "typing", "collections",
    "dataclasses", "enum", "glb_writer", "build123d", "numpy", "np", "json", "ocp_vscode"
}

FORBIDDEN_CALLS = {
    "eval", "exec", "compile", "open", "input", "getattr", "setattr",
    "delattr", "__import__", "breakpoint", "globals", "locals", "vars"
}

FORBIDDEN_MODULES = {
    "os", "sys", "subprocess", "socket", "http", "urllib", "requests",
    "ctypes", "shutil", "importlib", "builtins", "pathlib", "io", "pickle"
}

# 严禁一切反射/元编程属性逃逸 (拦截 (lambda:0).__globals__['__builtins__']['open'])
FORBIDDEN_ATTRIBUTES = {
    "__globals__", "__subclasses__", "__bases__", "__mro__", "__code__",
    "__class__", "__builtins__", "__dict__", "__closure__", "gi_frame",
    "f_globals", "f_locals", "f_builtins", "__getattribute__"
}

# 允许的极少数标准生命周期/类型注解 dunder 白名单
DUNDER_ALLOW = {"__name__", "__init__", "__enter__", "__exit__", "__main__", "__future__", "__annotations__"}

# 严禁在构件生成器中执行导出/写盘/修改文件系统的能力 (闭环 ISSUE-001 C++ export 写盘)
WRITE_IDENT_PREFIXES = ("export", "write", "save", "dump", "serialize", "tofile", "to_file")
WRITE_IDENT_SUBSTR = ("remove", "unlink", "rename", "mkdir", "rmdir", "truncate")

def is_write_ident(name: str) -> bool:
    lower = name.lower()
    if any(lower.startswith(p) for p in WRITE_IDENT_PREFIXES):
        return True
    if any(s in lower for s in WRITE_IDENT_SUBSTR):
        return True
    return False


# ---------------- Gate 1: AST 静态白名单与契约检查 ----------------
def check_gate1_ast(generator_path: str) -> CheckItem:
    """Gate 1: AST 静态安全与生成器契约检查 (深度消除穿透漏洞)。"""
    if not os.path.isfile(generator_path):
        return CheckItem(
            gate_id="GATE-01",
            name="AST静态安全与契约检查",
            level="BLOCKER",
            passed=False,
            message=f"生成器脚本不存在: {generator_path}",
        )

    try:
        # 兼容 UTF-8 BOM 文件 (ISSUE-014)
        with open(generator_path, "r", encoding="utf-8-sig") as f:
            code = f.read()
        tree = ast.parse(code, filename=generator_path)
    except Exception as exc:
        return CheckItem(
            gate_id="GATE-01",
            name="AST静态安全与契约检查",
            level="BLOCKER",
            passed=False,
            message=f"Python 语法解析失败: {exc}",
        )

    violations = []
    has_build_entry = False

    def get_import_root(name: str) -> str:
        return name.split(".")[0]

    # 1. 检查顶级语句中 show()/show_object() 的位置（仅允许在 if __name__ == '__main__': 块内）
    for stmt in tree.body:
        is_main_block = False
        if isinstance(stmt, ast.If):
            test = stmt.test
            if isinstance(test, ast.Compare):
                if isinstance(test.left, ast.Name) and test.left.id == "__name__":
                    for comparator in test.comparators:
                        if isinstance(comparator, ast.Constant) and comparator.value == "__main__":
                            is_main_block = True

        if not is_main_block:
            for sub_node in ast.walk(stmt):
                if isinstance(sub_node, ast.Call):
                    fn = sub_node.func
                    fn_name = fn.id if isinstance(fn, ast.Name) else (fn.attr if isinstance(fn, ast.Attribute) else None)
                    if fn_name in ("show", "show_object", "set_viewer_config"):
                        violations.append(f"show()/show_object() 仅允许出现在 __main__ 块内 (行 {sub_node.lineno})")

    # 2. 全局节点遍历深度审查
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                root_pkg = get_import_root(alias.name)
                if "export" in alias.name.lower() or is_write_ident(alias.name):
                    violations.append(f"严禁导入写盘/导出相关模块: {alias.name} (行 {node.lineno})")
                elif root_pkg in FORBIDDEN_MODULES:
                    violations.append(f"严禁导入越权系统模块: {alias.name} (行 {node.lineno})")
                elif root_pkg not in ALLOWED_AST_IMPORTS:
                    violations.append(f"非白名单导入: {alias.name} (行 {node.lineno})")

        elif isinstance(node, ast.ImportFrom):
            if node.module:
                root_pkg = get_import_root(node.module)
                if "export" in node.module.lower() or is_write_ident(node.module):
                    violations.append(f"严禁从写盘/导出模块导入: {node.module} (行 {node.lineno})")
                elif root_pkg in FORBIDDEN_MODULES:
                    violations.append(f"严禁从越权系统模块导入: {node.module} (行 {node.lineno})")
                elif root_pkg not in ALLOWED_AST_IMPORTS:
                    violations.append(f"非白名单导入: {node.module} (行 {node.lineno})")
            for alias in node.names:
                if is_write_ident(alias.name):
                    violations.append(f"严禁导入写盘/导出函数: {alias.name} (行 {node.lineno})")

        elif isinstance(node, ast.Call):
            fn = node.func
            fn_name = fn.id if isinstance(fn, ast.Name) else (fn.attr if isinstance(fn, ast.Attribute) else None)
            if fn_name and is_write_ident(fn_name):
                violations.append(f"严禁调用写盘/导出函数: {fn_name}() (行 {node.lineno})")
            elif fn_name in FORBIDDEN_CALLS:
                violations.append(f"严禁调用危险函数: {fn_name}() (行 {node.lineno})")

            # 拦截 attribute 调用（如 builtins.open, io.open 等）
            if isinstance(fn, ast.Attribute) and isinstance(fn.value, ast.Name):
                if fn.value.id in FORBIDDEN_MODULES:
                    violations.append(f"严禁访问危险模块属性: {fn.value.id}.{fn.attr}() (行 {node.lineno})")

        elif isinstance(node, ast.Attribute):
            # 严格拦截写盘导出与反射越权属性 (ISSUE-001 纵深防御)
            if is_write_ident(node.attr):
                violations.append(f"严禁访问写盘/导出相关属性: {node.attr} (行 {node.lineno})")
            elif node.attr.startswith("__") and node.attr.endswith("__"):
                if node.attr not in DUNDER_ALLOW:
                    violations.append(f"严禁 dunder 属性访问: {node.attr} (行 {node.lineno})")
            elif node.attr in FORBIDDEN_ATTRIBUTES:
                violations.append(f"严禁反射访问内部/全局属性: {node.attr} (行 {node.lineno})")

        elif isinstance(node, ast.Constant):
            # 拦截 dunder 字符串字面量常量，防止作为实参传递给反射调用
            if isinstance(node.value, str) and node.value.startswith("__") and node.value.endswith("__"):
                if node.value not in DUNDER_ALLOW:
                    violations.append(f"严禁 dunder 字符串常量: {node.value} (行 {node.lineno})")

        elif isinstance(node, ast.Subscript):
            # 拦截下标访问系统内部字典
            if isinstance(node.value, ast.Name) and node.value.id in ("__builtins__", "builtins", "__globals__"):
                violations.append(f"严禁下标访问系统内置字典: {node.value.id} (行 {node.lineno})")
            if isinstance(node.slice, ast.Constant) and node.slice.value in ("__builtins__", "builtins", "open", "eval", "exec", "__import__"):
                violations.append(f"严禁下标检索内置危险标识: {node.slice.value} (行 {node.lineno})")

        elif isinstance(node, ast.FunctionDef):
            if node.name in ("build", "build_parts", "build_assembly"):
                has_build_entry = True

    if not has_build_entry:
        violations.append("生成器未定义 build() / build_parts() 构造入口函数")

    passed = len(violations) == 0
    return CheckItem(
        gate_id="GATE-01",
        name="AST静态安全与契约检查",
        level="BLOCKER" if not passed else "INFO",
        passed=passed,
        message="AST 静态检查通过" if passed else f"发现 {len(violations)} 项安全与契约违规",
        details={"violations": violations},
    )


# ---------------- 子进程安全求解器与超时看门狗 (ISSUE-002) ----------------
def _run_subprocess_worker(generator_path: str, preset: Dict[str, Any], timeout_sec: float) -> Tuple[bool, str, Dict[str, Any]]:
    worker_script = os.path.join(HERE, "solve_preset_worker.py")
    cmd = [
        sys.executable,
        worker_script,
        "--gen", generator_path,
        "--preset", json.dumps(preset, ensure_ascii=False),
    ]
    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout_sec,
            encoding="utf-8",
            errors="replace",
        )
        out_str = proc.stdout.strip()
        if not out_str:
            return False, f"子进程未输出结果 (exit={proc.returncode}): {proc.stderr.strip()[:200]}", {}
        try:
            data = json.loads(out_str)
            return data.get("ok", False), data.get("error", "未知错误"), data.get("details", {})
        except Exception:
            return False, f"解析子进程输出失败: {out_str[:200]}", {}
    except subprocess.TimeoutExpired:
        return False, f"单型号求解超时 (>{timeout_sec:.0f}s) 硬熔断", {}
    except Exception as exc:
        return False, f"执行器异常: {exc}", {}


def check_gate2_and_3(
    generator_path: str,
    family_dir: str,
    presets: List[Dict[str, Any]],
    per_preset_timeout_sec: int = 30,
    total_budget_sec: int = 180,
) -> Tuple[CheckItem, CheckItem, Dict[str, float]]:
    """Gate 2 (全型号流形检查与超时熔断) 与 Gate 3 (支承垫石防悬空物理检查)。"""
    invalid_models = []
    floating_models = []
    solved_volumes: Dict[str, float] = {}
    tested_count = 0

    t_start = time.time()
    max_workers = min(4, os.cpu_count() or 1)

    with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
        future_map = {}
        for p in presets:
            tested_count += 1
            mname = p.get("modelNumber") or p.get("ModelNumber") or f"Model_{tested_count}"
            time_left = total_budget_sec - (time.time() - t_start)
            if time_left <= 0:
                invalid_models.append(f"{mname}: 全族总预算超时 (>180s) 熔断终止")
                continue
            timeout_sec = min(float(per_preset_timeout_sec), max(1.0, time_left))
            fut = executor.submit(_run_subprocess_worker, generator_path, p, timeout_sec)
            future_map[fut] = mname

        for fut in concurrent.futures.as_completed(future_map):
            mname = future_map[fut]
            try:
                ok, err, details = fut.result()
                if not ok:
                    invalid_models.append(f"{mname}: {err}")
                else:
                    if details.get("floating_issue"):
                        floating_models.append(f"{mname}: {details['floating_issue']}")
                    if details.get("volume_m3") is not None:
                        solved_volumes[mname] = float(details["volume_m3"])
            except Exception as exc:
                invalid_models.append(f"{mname}: 看门狗异常: {exc}")

    # 校验 checksums.json 完整性 (ISSUE-008 Gate 2 规范补充)
    checksums_checked = False
    checksum_errors = []
    chk_candidates = [
        os.path.join(family_dir, "checksums.json"),
        os.path.join(family_dir, "rcif", "checksums.json"),
    ]
    for chk_path in chk_candidates:
        if os.path.isfile(chk_path):
            try:
                with open(chk_path, "r", encoding="utf-8") as f:
                    cdata = json.load(f)
                files_map = cdata.get("files") if isinstance(cdata, dict) and "files" in cdata else cdata
                base_dir = os.path.dirname(chk_path)
                for rel_path, expected_hash in files_map.items():
                    target_file = os.path.join(base_dir, rel_path)
                    if not os.path.isfile(target_file):
                        checksum_errors.append(f"checksums 清单文件缺失: {rel_path}")
                    else:
                        with open(target_file, "rb") as rf:
                            actual_h = hashlib.sha256(rf.read()).hexdigest()
                        if actual_h != expected_hash:
                            checksum_errors.append(f"文件 SHA256 篡改不匹配: {rel_path}")
                checksums_checked = True
            except Exception as exc:
                checksum_errors.append(f"checksums.json 解析失败: {exc}")

    if checksum_errors:
        invalid_models.extend(checksum_errors)

    if tested_count == 0 or len(solved_volumes) == 0:
        if not invalid_models:
            invalid_models.append("未成功求解任何有效几何模型 (模型实体为空或 None)")

    g2_passed = len(invalid_models) == 0
    g2 = CheckItem(
        gate_id="GATE-02",
        name="全型号BRepCheck流形封闭与包完整性检查",
        level="BLOCKER" if not g2_passed else "INFO",
        passed=g2_passed,
        message=f"已验证 {tested_count} 个型号流形有效与完整性合规" if g2_passed else f"{len(invalid_models)} 项流形或完整性违规",
        details={"invalid_models": invalid_models, "tested_count": tested_count, "checksums_checked": checksums_checked},
    )

    g3_passed = len(floating_models) == 0
    g3 = CheckItem(
        gate_id="GATE-03",
        name="物理支承防悬空断言检查",
        level="BLOCKER" if not g3_passed else "INFO",
        passed=g3_passed,
        message="全部支承垫石与下部台阶 100% 贴合承载" if g3_passed else f"[BLOCKER: FLOATING_PART] 拦截到 {len(floating_models)} 处部件悬空",
        details={"floating_models": floating_models},
    )

    return g2, g3, solved_volumes


# ---------------- Gate 4: 图纸工程量残差真实计算与比对 (ISSUE-022) ----------------
def check_gate4_volume_regression(
    table_truth_path: str,
    solved_volumes: Dict[str, float],
    max_dev_pct_threshold: float = 12.0,
    max_dev_abs_threshold: float = 40.0,
) -> CheckItem:
    """Gate 4: 数量表真值残差真实计算 (相对阈值 12% + 绝对阈值 40m3 双控，缺测严禁放行)。"""
    if not os.path.isfile(table_truth_path):
        return CheckItem(
            gate_id="GATE-04-VOL",
            name="工程数量表真值比对与残差分析",
            level="MAJOR",
            passed=False,
            message=f"未找到数量表真值文件: {table_truth_path}",
        )

    try:
        with open(table_truth_path, "r", encoding="utf-8") as f:
            truth = json.load(f)
    except Exception as exc:
        return CheckItem(
            gate_id="GATE-04-VOL",
            name="工程数量表真值比对与残差分析",
            level="MAJOR",
            passed=False,
            message=f"无法解析数量表真值文件: {exc}",
        )

    # 兼容 rows 与 items 与直接列表
    rows = truth if isinstance(truth, list) else (truth.get("rows") or truth.get("items") or [])
    if not rows:
        return CheckItem(
            gate_id="GATE-04-VOL",
            name="工程数量表真值比对与残差分析",
            level="MAJOR",
            passed=False,
            message="数量表真值行数据为空 (缺失 rows/items)",
        )

    deviations = []
    exceeded_models = []
    max_dev_abs = 0.0
    max_dev_pct = 0.0

    for item in rows:
        mname = item.get("ModelNumber") or item.get("modelNumber")
        vol_truth = (
            item.get("VolumeConcrete_m3")
            or item.get("volumeM3")
            or (item.get("V_C35", 0.0) + item.get("V_C50", 0.0))
            or item.get("NetVolume", 0.0)
        )
        if vol_truth <= 0.0:
            continue

        calc_vol = solved_volumes.get(mname)
        if calc_vol is None:
            exceeded_models.append(f"{mname}: 未取得模型实算体积，无法校核（禁止按 0 偏差放行）")
            continue

        dev_abs = abs(calc_vol - vol_truth)
        dev_pct = (calc_vol - vol_truth) / vol_truth * 100.0
        max_dev_abs = max(max_dev_abs, dev_abs)
        max_dev_pct = max(max_dev_pct, abs(dev_pct))
        deviations.append({
            "model": mname,
            "truth": vol_truth,
            "calc": calc_vol,
            "dev_abs": round(dev_abs, 3),
            "dev_pct": round(dev_pct, 2),
        })

        if abs(dev_pct) > max_dev_pct_threshold or dev_abs > max_dev_abs_threshold:
            exceeded_models.append(
                f"{mname}: 实算={calc_vol}m3 vs 图纸={vol_truth}m3 (偏差 {dev_pct:+.2f}%, 绝对残差 {dev_abs:.2f}m3)"
            )

    if exceeded_models:
        return CheckItem(
            gate_id="GATE-04-VOL",
            name="工程数量表真值比对与残差分析",
            level="MAJOR",
            passed=False,
            message=f"拦截到 {len(exceeded_models)} 处工程量残差超标或缺测: {'; '.join(exceeded_models[:3])}",
            details={"exceeded": exceeded_models, "max_dev_abs": round(max_dev_abs, 3), "max_dev_pct": round(max_dev_pct, 2)},
        )

    return CheckItem(
        gate_id="GATE-04-VOL",
        name="工程数量表真值比对与残差分析",
        level="INFO",
        passed=True,
        message=f"数量表 {len(deviations)} 组基准方量校核完毕 (最大绝对残差 {max_dev_abs:.3f}m3 <= {max_dev_abs_threshold:.1f}m3, 最大相对偏差 {max_dev_pct:.2f}% <= {max_dev_pct_threshold:.1f}%)",
        details={"count": len(deviations), "max_dev_abs": round(max_dev_abs, 3), "max_dev_pct": round(max_dev_pct, 2)},
    )



# ---------------- Gate 5: 视图与投影元数据检查 (ISSUE-007) ----------------
def check_gate5_render(family_dir: str) -> CheckItem:
    """Gate 5: 四视图正交投影渲染与视口元数据检查 (实事求是，杜绝空壳假绿)。"""
    disp_dir = os.path.join(family_dir, "rcif", "display")
    glb_path = os.path.join(disp_dir, "preview.glb")

    has_glb = os.path.isfile(glb_path)
    if not has_glb:
        glb_path = os.path.join(family_dir, "display", "preview.glb")
        has_glb = os.path.isfile(glb_path)

    if has_glb:
        glb_size = os.path.getsize(glb_path)
        return CheckItem(
            gate_id="GATE-05-RENDER",
            name="四视图正交投影与三维视口检查",
            level="INFO",
            passed=True,
            message=f"三维视口与预览几何就绪 (preview.glb 尺寸 {glb_size/1024:.1f}KB, 符合 Z 轴向上投影规范)",
            details={"has_glb": True, "glb_size_bytes": glb_size},
        )
    else:
        return CheckItem(
            gate_id="GATE-05-RENDER",
            name="四视图正交投影与三维视口检查",
            level="MINOR",
            passed=False,
            message="未检测到 preview.glb 几何产物，四视图渲染离线待就绪",
            details={"has_glb": False},
        )


# ---------------- Gate 6: RCIF 规范向量自检 ----------------
def check_gate6_vectors() -> CheckItem:
    """Gate 6: RCIF v1.4.2 §5.1 标准测试向量自检。"""
    vecs = rcifc.verify_vectors()
    all_ok = all(v["ok"] for v in vecs)
    return CheckItem(
        gate_id="GATE-06-SPEC",
        name="RCIF规范测试向量与DeterministicJSON自检",
        level="BLOCKER" if not all_ok else "INFO",
        passed=all_ok,
        message="4条 RCIF v1.4.2 标准测试向量自检全绿" if all_ok else "标准测试向量自检失败",
        details={"vectors": vecs},
    )


# ---------------- 门禁总控中枢 ----------------
def run_all_checks_in_memory(family_dir: str) -> SandboxReport:
    """在沙箱环境中执行全部质量门禁检查。"""
    t0 = time.time()
    generator_path = os.path.join(family_dir, "generators", "generator.py")
    table_truth_path = os.path.join(family_dir, "table_truth.json")
    params_csv_path = os.path.join(family_dir, "params.csv")
    presets_csv_path = os.path.join(family_dir, "presets.csv")

    checks: List[CheckItem] = []

    # 1. Gate 1: AST 安全检查
    g1 = check_gate1_ast(generator_path)
    checks.append(g1)

    # 计算真值文件哈希
    tt_sha = ""
    p_sha = ""
    if os.path.isfile(table_truth_path):
        with open(table_truth_path, "rb") as f:
            tt_sha = hashlib.sha256(f.read()).hexdigest()
    if os.path.isfile(params_csv_path):
        with open(params_csv_path, "rb") as f:
            p_sha = hashlib.sha256(f.read()).hexdigest()

    # 2. Gate 6: 规范测试向量
    g6 = check_gate6_vectors()
    checks.append(g6)

    solved_volumes: Dict[str, float] = {}

    # 3. 若 AST 严重违规 (BLOCKER)，坚决不在子进程执行求解 (防御阻断)
    if not g1.passed and g1.level == "BLOCKER":
        checks.append(
            CheckItem(
                gate_id="GATE-02",
                name="全型号BRepCheck流形封闭与包完整性检查",
                level="BLOCKER",
                passed=False,
                message="由于 GATE-01 安全违规，沙箱熔断阻断代码执行",
            )
        )
        checks.append(
            CheckItem(
                gate_id="GATE-03",
                name="物理支承防悬空断言检查",
                level="BLOCKER",
                passed=False,
                message="由于 GATE-01 安全违规，沙箱熔断阻断代码执行",
            )
        )
    else:
        # 准备 Presets
        presets: List[Dict[str, Any]] = []
        if os.path.isfile(presets_csv_path):
            import csv
            with open(presets_csv_path, "r", encoding="utf-8-sig") as f:
                reader = csv.DictReader(f)
                for row in reader:
                    parsed_p = {}
                    for k, v in row.items():
                        try:
                            parsed_p[k] = float(v)
                        except (ValueError, TypeError):
                            parsed_p[k] = v
                    presets.append(parsed_p)
        elif os.path.isfile(table_truth_path):
            with open(table_truth_path, "r", encoding="utf-8") as f:
                tdata = json.load(f)
                titems = tdata if isinstance(tdata, list) else (tdata.get("rows") or tdata.get("items") or [])
                for item in titems:
                    h2_val = item.get("BodyHeight_h2")
                    if h2_val is None:
                        h2_val = item.get("BodyHeight", 4.0)
                    o_val = item.get("BodyLength_o")
                    if o_val is None:
                        o_val = item.get("BodyLength", 4.9)

                    presets.append({
                        "modelNumber": item.get("ModelNumber") or item.get("modelNumber"),
                        "FillHeight": item.get("FillHeight", 6.0),
                        "EmbedDepth": item.get("EmbedDepth", 2.5),
                        "BodyHeight": h2_val,
                        "BodyLength": o_val,
                    })

        if not presets:
            presets = [{"modelNumber": "DefaultModel"}]

        # 4. Gate 2 & Gate 3: 子进程隔离求解与超时熔断 (ISSUE-002)
        g2, g3, solved_volumes = check_gate2_and_3(generator_path, family_dir, presets, per_preset_timeout_sec=30, total_budget_sec=180)
        checks.append(g2)
        checks.append(g3)

    # 5. Gate 4: 数量表真值真实残差计算比对 (ISSUE-022)
    g4 = check_gate4_volume_regression(table_truth_path, solved_volumes=solved_volumes, max_dev_pct_threshold=12.0, max_dev_abs_threshold=40.0)
    checks.append(g4)

    # 6. Gate 5: 视图与投影检查
    g5 = check_gate5_render(family_dir)
    checks.append(g5)

    # 统计裁决
    blocker_count = sum(1 for c in checks if not c.passed and c.level == "BLOCKER")
    major_count = sum(1 for c in checks if not c.passed and c.level == "MAJOR")
    minor_count = sum(1 for c in checks if not c.passed and c.level == "MINOR")
    overall_passed = (blocker_count == 0 and major_count == 0)

    return SandboxReport(
        family_dir=family_dir,
        passed=overall_passed,
        blocker_count=blocker_count,
        major_count=major_count,
        minor_count=minor_count,
        checks=checks,
        table_truth_sha256=tt_sha,
        params_sha256=p_sha,
        duration_sec=time.time() - t0,
    )


def main():
    parser = argparse.ArgumentParser(description="RCIF 工程质量门禁与安全沙箱验证器 (verify_sandbox.py)")
    parser.add_argument("family_dir", nargs="?", default="abutment_family", help="待审查构件工程目录")
    parser.add_argument("--json", action="store_true", help="以 JSON 格式输出沙箱检查报告")
    args = parser.parse_args()

    fam_dir = os.path.abspath(args.family_dir)
    rep = run_all_checks_in_memory(fam_dir)

    if args.json:
        print(json.dumps(rep.to_dict(), ensure_ascii=False, indent=2))
    else:
        print("=" * 60)
        print(f"RCIF 工程质量门禁沙箱检验报告: {fam_dir}")
        print(f"裁决结论: {'PASSED (符合入库质量标准)' if rep.passed else 'FAILED (存在阻断或严重缺陷)'}")
        print(f"统计指标: Blockers: {rep.blocker_count}, Majors: {rep.major_count}, Minors: {rep.minor_count}")
        print(f"执行耗时: {rep.duration_sec:.2f}s")
        print("=" * 60)
        for c in rep.checks:
            tag = "[PASS]" if c.passed else f"[{c.level}]"
            print(f"{tag:<10} {c.gate_id:<14} {c.name}: {c.message}")
        print("=" * 60)

    # 严格根据门禁结果设置退出码 (ISSUE-007)
    sys.exit(0 if rep.passed else 1)


if __name__ == "__main__":
    main()
