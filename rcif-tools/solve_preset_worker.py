"""参数化构件子进程独立沙箱求解执行器 (solve_preset_worker.py)
由 verify_sandbox.py 在独立 OS 进程中启动，施加进程级物理隔离与超时硬熔断。
底层强制覆写模块的 __builtins__ 为受限字典，并接入 PEP 578 原生审计钩子，物理斩断一切系统与写盘逃逸 (ISSUE-001)。
"""

from __future__ import annotations

import argparse
import builtins as _raw_bi
import importlib.util
import json
import os
import sys

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

# 避免导入期写 .pyc 触发误杀
sys.dont_write_bytecode = True

# ---------------- PEP 578 原生底层安全审计钩子 (物理硬拦截) ----------------
# 禁写 + 禁网 + 禁子进程；保障 cadquery 依赖的 OpenCASCADE C++ 几何引擎正常运行
_DENY_EVENTS = ("os.system", "os.exec", "subprocess.", "socket.", "shutil.", "posix.", "winreg.")

def _sandbox_audithook(event, args):
    if event == "open":
        path = args[0] if len(args) > 0 else ""
        mode = args[1] if len(args) > 1 else ""
        flags = args[2] if len(args) > 2 else 0
        if mode and any(ch in str(mode) for ch in "wax+"):
            raise PermissionError(f"[SANDBOX] 沙箱禁止一切写文件操作: {path}")
        if isinstance(flags, int):
            write_flags = os.O_WRONLY | os.O_RDWR | getattr(os, "O_CREAT", 0) | getattr(os, "O_TRUNC", 0)
            if flags & write_flags:
                raise PermissionError(f"[SANDBOX] 沙箱禁止底层写文件操作 (flags={flags}): {path}")
    elif any(event.startswith(p) for p in _DENY_EVENTS):
        raise PermissionError(f"[SANDBOX] 沙箱禁止调用危险系统操作: {event}")

try:
    sys.addaudithook(_sandbox_audithook)
except Exception:
    pass

def install_export_mocks():
    """在运行时将所有 CadQuery 及 C++ 底层导出接口替换为直接抛错桩，阻断 C++ 导出写盘 (ISSUE-001)。"""
    try:
        import cadquery as _cq
        def _deny_export(*args, **kwargs):
            raise PermissionError("[SANDBOX] 沙箱严禁在生成器中执行任何导出/写盘操作 (C++ export 已阻断)")

        # 1. 拦截 cadquery.exporters 下的所有导出接口
        if hasattr(_cq, "exporters"):
            for attr in ("export", "exportShape", "exportSTEP", "exportSTL", "exportAMF", "exportSVG", "exportDXF"):
                if hasattr(_cq.exporters, attr):
                    setattr(_cq.exporters, attr, _deny_export)

        # 2. 拦截 cadquery.occ_impl.shapes.Shape 下的导出接口
        try:
            from cadquery.occ_impl.shapes import Shape as _CqShape
            for attr in ("exportStep", "exportStl", "exportBrep", "exportShape"):
                if hasattr(_CqShape, attr):
                    setattr(_CqShape, attr, _deny_export)
        except Exception:
            pass

        # 3. 拦截 Workplane 的导出接口
        if hasattr(_cq, "Workplane"):
            for attr in ("exportStep", "exportStl"):
                if hasattr(_cq.Workplane, attr):
                    setattr(_cq.Workplane, attr, _deny_export)
    except Exception:
        pass

# 受限 builtins 安全白名单
SAFE_BUILTIN_NAMES = {
    "abs", "all", "any", "bool", "dict", "enumerate", "float", "int", "isinstance",
    "len", "list", "max", "min", "pow", "range", "round", "set", "sorted", "str",
    "sum", "tuple", "zip", "ValueError", "RuntimeError", "KeyError", "Exception",
    "TypeError", "AttributeError", "IndexError", "StopIteration", "True", "False", "None"
}

ALLOWED_WORKER_MODULES = {
    "cadquery", "cq", "math", "glb_writer", "numpy", "np", "json",
    "typing", "collections", "dataclasses", "enum", "build123d", "__future__", "ocp_vscode"
}


def _safe_import(name, *args, **kwargs):
    root_pkg = name.split(".")[0]
    if root_pkg not in ALLOWED_WORKER_MODULES:
        raise ImportError(f"沙箱安全拦截：禁止导入非白名单模块: {name}")
    return _raw_bi.__import__(name, *args, **kwargs)


def get_safe_builtins_dict():
    safe_dict = {k: getattr(_raw_bi, k) for k in SAFE_BUILTIN_NAMES if hasattr(_raw_bi, k)}
    safe_dict["__import__"] = _safe_import
    return safe_dict


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gen", required=True, help="生成器文件路径")
    parser.add_argument("--preset", required=True, help="型号参数 JSON 字符串")
    args = parser.parse_args()

    gen_path = os.path.abspath(args.gen)
    gen_dir = os.path.dirname(gen_path)
    if gen_dir not in sys.path:
        sys.path.insert(0, gen_dir)

    tools_dir = os.path.dirname(os.path.abspath(__file__))
    if tools_dir not in sys.path:
        sys.path.insert(0, tools_dir)

    try:
        preset_dict = json.loads(args.preset)
    except Exception as exc:
        print(json.dumps({"ok": False, "error": f"参数 JSON 解析失败: {exc}"}, ensure_ascii=False))
        sys.exit(1)

    try:
        spec = importlib.util.spec_from_file_location("worker_generator_mod", gen_path)
        if spec is None or spec.loader is None:
            print(json.dumps({"ok": False, "error": f"无法加载模块: {gen_path}"}, ensure_ascii=False))
            sys.exit(1)
        mod = importlib.util.module_from_spec(spec)

        # 核心防线：在 exec_module 执行前强行注入受限 builtins 字典与 C++ 导出阻断桩 (ISSUE-001)
        mod.__dict__["__builtins__"] = get_safe_builtins_dict()
        install_export_mocks()

        spec.loader.exec_module(mod)

        floating_issue = None
        calculated_vol = None

        if hasattr(mod, "build_parts"):
            parts = mod.build_parts(preset_dict)
            if not isinstance(parts, dict) or not parts:
                print(json.dumps({"ok": False, "error": "build_parts 未返回任何有效几何部件字典 (空模型)"}, ensure_ascii=False))
                sys.exit(0)

            valid_parts_count = 0
            for part_name, shape in parts.items():
                if shape is None:
                    continue
                s = shape.val() if hasattr(shape, "val") else shape
                if s is None:
                    continue
                if hasattr(s, "isValid") and not s.isValid():
                    print(json.dumps({"ok": False, "error": f"{part_name} 几何非流形"}, ensure_ascii=False))
                    sys.exit(0)
                valid_parts_count += 1

            if valid_parts_count == 0:
                print(json.dumps({"ok": False, "error": "build_parts 所有部件实体均为 None (空模型)"}, ensure_ascii=False))
                sys.exit(0)

            # Gate 3 防悬空检查
            pad_l = parts.get("BearingPad/Left") or parts.get("BearingPadLeft")
            cap = parts.get("Cap")
            if pad_l is not None and cap is not None:
                bb_pad = pad_l.val().BoundingBox() if hasattr(pad_l, "val") else pad_l.BoundingBox()
                bb_cap = cap.val().BoundingBox() if hasattr(cap, "val") else cap.BoundingBox()
                if bb_pad.ymin < bb_cap.ymin - 5.0 or bb_pad.ymax > bb_cap.ymax + 5.0:
                    floating_issue = f"垫石顺桥向 [{bb_pad.ymin:.1f}, {bb_pad.ymax:.1f}] 超出顶帽台阶 [{bb_cap.ymin:.1f}, {bb_cap.ymax:.1f}]"

            # 汇总主体总方量 (排除明挖基础 Footing，对应图纸《台身尺寸及工程数量表》)
            total_vol_mm3 = 0.0
            for part_name, pshape in parts.items():
                if part_name == "Footing":
                    continue
                s = pshape.val() if hasattr(pshape, "val") else pshape
                if hasattr(s, "Volume"):
                    total_vol_mm3 += s.Volume()
            if total_vol_mm3 > 0:
                calculated_vol = round(total_vol_mm3 / 1e9, 2)
            else:
                print(json.dumps({"ok": False, "error": "各有效部件计算体积总和为 0"}, ensure_ascii=False))
                sys.exit(0)

        elif hasattr(mod, "build"):
            shape = mod.build(preset_dict)
            if shape is None:
                print(json.dumps({"ok": False, "error": "build 未返回几何实体 (返回了 None)"}, ensure_ascii=False))
                sys.exit(0)
            s = shape.val() if hasattr(shape, "val") else shape
            if s is None:
                print(json.dumps({"ok": False, "error": "build 实体为 None"}, ensure_ascii=False))
                sys.exit(0)
            if hasattr(s, "isValid") and not s.isValid():
                print(json.dumps({"ok": False, "error": "几何非流形"}, ensure_ascii=False))
                sys.exit(0)
            if hasattr(s, "Volume"):
                vol = s.Volume()
                if vol <= 0:
                    print(json.dumps({"ok": False, "error": "build 几何体积为 0"}, ensure_ascii=False))
                    sys.exit(0)
                calculated_vol = round(vol / 1e9, 2)
            else:
                print(json.dumps({"ok": False, "error": "实体未暴露 Volume 属性"}, ensure_ascii=False))
                sys.exit(0)
        else:
            print(json.dumps({"ok": False, "error": "未找到可用 build / build_parts 函数"}, ensure_ascii=False))
            sys.exit(0)

        result = {
            "ok": True,
            "error": "",
            "details": {
                "floating_issue": floating_issue,
                "volume_m3": calculated_vol,
            },
        }
        print(json.dumps(result, ensure_ascii=False))
        sys.exit(0)

    except Exception as exc:
        print(json.dumps({"ok": False, "error": f"求解异常: {exc}"}, ensure_ascii=False))
        sys.exit(0)


if __name__ == "__main__":
    main()
