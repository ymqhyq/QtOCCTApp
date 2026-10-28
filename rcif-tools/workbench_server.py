"""RCIF 构件构建工作台后端服务 (FastAPI / Uvicorn)

⚠️ DEPRECATED（开发/本机演示专用）
DSH 宿主插件 (`dsh-rcif/dist/index.js`) 已实现同一组 `/rcif-studio/api/*` 路由，
并带 owner Peer 准入、capability token、Ed25519 审批与路径收敛。本文件仅用于
未启动 DSH 时的本机调试，**不得**用于生产或多用户环境：无鉴权、无审批签名。
新部署请优先使用 DSH 插件（见 `dsh-rcif/README.md`）。

特性：
- 严格路径防穿越校验 (resolve_in_root)
- CORS 白名单限制，杜绝通配符越权
- 纯内存/安全临时文件管理，不污染仓库根目录
- 统一请求异常捕获与状态码规范
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request
import uuid
from datetime import datetime, timezone

from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse, RedirectResponse, Response
from fastapi.staticfiles import StaticFiles
from starlette.concurrency import run_in_threadpool

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.realpath(os.path.dirname(HERE))

if HERE not in sys.path:
    sys.path.insert(0, HERE)

import package_api
import pack_rcif_cli
import rcifc
import verify_sandbox

app = FastAPI(title="RCIF Workbench Server", version="1.4.2")

# 当前工作台构件包（进程内状态；DSH 插件使用同样的语义）
CURRENT_PACKAGE = None
DSH_PORT = int(os.environ.get("RCIF_DSH_PORT", "3090"))
DSH_URL = os.environ.get("RCIF_DSH_URL", f"http://127.0.0.1:{DSH_PORT}/")


def assistant_running() -> bool:
    try:
        with urllib.request.urlopen(DSH_URL, timeout=1.5) as response:
            return response.status < 500
    except urllib.error.HTTPError:
        # 401 表示 DSH 已启动、只是需要浏览器登录 Cookie
        return True
    except Exception:
        return False


def recent_session_ids() -> list[dict]:
    base = os.path.join(os.path.expanduser("~"), ".dsh", "sessions")
    slug = "--" + re.sub(r"[:\\/]+", "-", ROOT_DIR).strip("-") + "--"
    candidates = [os.path.join(base, slug), base]
    for directory in candidates:
        if not os.path.isdir(directory):
            continue
        rows = []
        for name in os.listdir(directory):
            path = os.path.join(directory, name)
            if not os.path.isdir(path):
                continue
            rows.append({
                "sessionId": name,
                "title": name,
                "updatedAt": time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(os.path.getmtime(path))),
                "running": False,
                "agentAvailable": False,
                "hasPackage": CURRENT_PACKAGE is not None,
                "packagePath": CURRENT_PACKAGE,
            })
        rows.sort(key=lambda item: item["updatedAt"], reverse=True)
        user_rows = [row for row in rows if row["sessionId"].startswith("session-")]
        return user_rows if user_rows else rows
    return []


# 收紧 CORS 白名单 (ISSUE-005)
app.add_middleware(
    CORSMiddleware,
    allow_origin_regex=r"^https?://(localhost|127\.0\.0\.1)(:\d+)?$",
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


def resolve_in_root(raw_path: str) -> str:
    """严格校验路径是否在平台根工作区内，杜绝路径遍历 (ISSUE-005)。"""
    if not raw_path:
        return ROOT_DIR
    target = os.path.realpath(raw_path if os.path.isabs(raw_path) else os.path.join(ROOT_DIR, raw_path))
    try:
        common = os.path.commonpath([ROOT_DIR, target])
        if common != ROOT_DIR:
            raise ValueError(f"越权访问路径: {raw_path}")
    except Exception:
        raise ValueError(f"非法路径: {raw_path}")
    return target


def resolve_package(raw_path: str) -> str:
    target = resolve_in_root(raw_path)
    if not target.lower().endswith(".rcif"):
        raise ValueError("只接受 .rcif 构件包")
    if not os.path.isfile(target):
        raise FileNotFoundError(f"构件包不存在: {target}")
    return target


def default_family_id(kind: str, source_path: str) -> str:
    base = os.path.splitext(os.path.basename(source_path))[0]
    cleaned = "".join(ch if (ch.isalnum() or ch == "_") else "_" for ch in base).strip("_")
    if not cleaned or not cleaned[0].isalpha():
        cleaned = f"Imported_{cleaned or kind}"
    return cleaned[:32]


ASSISTANT_ROOT = os.path.join(ROOT_DIR, "铁路构件智能助手")
os.makedirs(ASSISTANT_ROOT, exist_ok=True)


def session_meta_file(workspace_path: str, session_id: str) -> str:
    safe = "".join(ch if (ch.isalnum() or ch in "._-") else "_" for ch in session_id)[:128]
    return os.path.join(workspace_path, ".rcif", "sessions", f"{safe}.json")


def find_session_workspace(session_id: str):
    for entry in os.scandir(ASSISTANT_ROOT):
        if not entry.is_dir():
            continue
        candidate = session_meta_file(entry.path, session_id)
        if os.path.isfile(candidate):
            return entry.path, candidate
    return None, None


def read_session_meta(session_id: str):
    workspace_path, candidate = find_session_workspace(session_id)
    if not candidate:
        return None
    try:
        with open(candidate, encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        data = {}
    return {"workspacePath": workspace_path, **data}


def write_session_meta(session_id: str, patch: dict):
    workspace_path, candidate = find_session_workspace(session_id)
    if not candidate:
        raise HTTPException(status_code=404, detail="会话不属于助手工作区")
    try:
        with open(candidate, encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        data = {}
    data["sessionId"] = session_id
    data["workspacePath"] = workspace_path
    data["updatedAt"] = datetime.now(timezone.utc).isoformat()
    for key in ("inputFile", "componentName", "outputPath", "packagePath", "sourceKind", "revision"):
        if key in patch and isinstance(patch[key], str):
            data[key] = patch[key][:1000]
    os.makedirs(os.path.dirname(candidate), exist_ok=True)
    temp = f"{candidate}.tmp"
    with open(temp, "w", encoding="utf-8") as handle:
        json.dump(data, handle, ensure_ascii=False, indent=2)
    os.replace(temp, candidate)
    return data


@app.get("/rcif-studio/api/tree")
async def tree():
    workspaces = []
    for entry in os.scandir(ASSISTANT_ROOT):
        if not entry.is_dir():
            continue
        sessions = []
        sessions_dir = os.path.join(entry.path, ".rcif", "sessions")
        if os.path.isdir(sessions_dir):
            for name in os.listdir(sessions_dir):
                if not name.endswith(".json"):
                    continue
                session_id = name[:-5]
                try:
                    with open(os.path.join(sessions_dir, name), encoding="utf-8") as handle:
                        meta = json.load(handle)
                except (OSError, ValueError):
                    meta = {}
                sessions.append({
                    "sessionId": session_id,
                    "title": meta.get("componentName") or session_id,
                    "updatedAt": meta.get("updatedAt"),
                    "running": False,
                    "agentAvailable": False,
                    "hasPackage": bool(meta.get("packagePath")),
                    "meta": {"workspacePath": entry.path, **meta},
                })
        sessions.sort(key=lambda item: item.get("updatedAt") or "", reverse=True)
        workspaces.append({
            "workspaceId": entry.name,
            "title": entry.name,
            "path": entry.path,
            "updatedAt": sessions[0]["updatedAt"] if sessions else None,
            "sessions": sessions,
        })
    workspaces.sort(key=lambda item: item.get("updatedAt") or "", reverse=True)
    return {"ok": True, "root": ASSISTANT_ROOT, "workspaces": workspaces, "defaultSessionId": "session-default"}


@app.post("/rcif-studio/api/workspaces")
async def create_workspace(request: Request):
    body = await request.json()
    name = str(body.get("name") or "").strip()
    if not name or name in (".", "..") or any(ch in name for ch in '<>:"/\\|?*'):
        raise HTTPException(status_code=422, detail="工作区名称非法")
    path = os.path.realpath(os.path.join(ASSISTANT_ROOT, name))
    if os.path.dirname(path) != os.path.realpath(ASSISTANT_ROOT):
        raise HTTPException(status_code=403, detail="工作区路径越界")
    os.makedirs(os.path.join(path, ".rcif", "sessions"), exist_ok=True)
    return {"ok": True, "created": True, "workspace": {"workspaceId": name, "title": name, "path": path, "sessionIds": []}}


@app.post("/rcif-studio/api/sessions")
async def create_session(request: Request):
    raise HTTPException(status_code=501, detail="调试后端不能创建 DSH 会话；请使用 DSH 工作台")


@app.get("/rcif-studio/api/session-files")
async def session_files(sessionId: str):
    session_info = read_session_meta(sessionId)
    if not session_info:
        raise HTTPException(status_code=404, detail="会话不属于助手工作区")
    inputs_dir = os.path.join(session_info["workspacePath"], "inputs")
    files = []
    if os.path.isdir(inputs_dir):
        for name in os.listdir(inputs_dir):
            if not re.search(r"\.(pdf|step|stp|ifc)$", name, re.IGNORECASE):
                continue
            target = os.path.join(inputs_dir, name)
            if not os.path.isfile(target):
                continue
            stat = os.stat(target)
            files.append({
                "path": f"inputs/{name}",
                "name": name,
                "bytes": stat.st_size,
                "updatedAt": datetime.fromtimestamp(stat.st_mtime, timezone.utc).isoformat(),
            })
    files.sort(key=lambda item: item["updatedAt"], reverse=True)
    return {"ok": True, "sessionId": sessionId, "sessionDir": session_info["workspacePath"], "inputsDir": "inputs", "files": files}


@app.get("/rcif-studio/api/session-meta")
async def session_meta(sessionId: str):
    meta = read_session_meta(sessionId)
    if meta is None:
        raise HTTPException(status_code=404, detail="会话不属于助手工作区")
    return {"ok": True, "workspace": {"workspaceId": os.path.basename(meta["workspacePath"]), "title": os.path.basename(meta["workspacePath"]), "path": meta["workspacePath"]}, "meta": meta}


@app.put("/rcif-studio/api/session-meta")
async def update_session_meta(request: Request):
    body = await request.json()
    session_id = str(body.get("sessionId") or "")
    if not session_id:
        raise HTTPException(status_code=422, detail="缺少 sessionId")
    meta = write_session_meta(session_id, body)
    return {"ok": True, "meta": meta}


@app.post("/rcif-studio/api/upload")
async def upload(request: Request, sessionId: str, name: str):
    session_info = read_session_meta(sessionId)
    if not session_info:
        raise HTTPException(status_code=404, detail="会话不属于助手工作区")
    safe_name = os.path.basename(name)
    if not re.search(r"\.(pdf|step|stp|ifc)$", safe_name, re.IGNORECASE):
        raise HTTPException(status_code=422, detail="只接受 .pdf/.step/.stp/.ifc 文件")
    payload = await request.body()
    if not payload:
        raise HTTPException(status_code=422, detail="上传文件为空")
    inputs_dir = os.path.join(session_info["workspacePath"], "inputs")
    os.makedirs(inputs_dir, exist_ok=True)
    target = os.path.join(inputs_dir, safe_name)
    with open(target, "wb") as handle:
        handle.write(payload)
    relative = os.path.relpath(target, session_info["workspacePath"])
    kind = "pdf" if safe_name.lower().endswith(".pdf") else "ifc" if safe_name.lower().endswith(".ifc") else "step"
    meta = write_session_meta(sessionId, {"inputFile": relative, "sourceKind": kind})
    return {"ok": True, "path": relative, "bytes": len(payload), "sessionInfo": meta}


@app.get("/rcif-studio/api/state")
async def state(sessionId: str | None = None):
    session_info = read_session_meta(sessionId) if sessionId else None
    package_path = (session_info or {}).get("packagePath") or CURRENT_PACKAGE
    meta = None
    if package_path and os.path.isfile(package_path):
        try:
            meta = package_api.read_package(package_path)
        except Exception:  # noqa: BLE001
            meta = None
    return {
        "ok": True,
        "sessionId": sessionId or "session-default",
        "packagePath": package_path,
        "updatedAt": meta["updatedAt"] if meta else None,
        "packageSha256": meta["packageSha256"] if meta else None,
        "metadata": meta["metadata"] if meta else None,
        "workspace": {"workspaceId": os.path.basename(session_info["workspacePath"]), "title": os.path.basename(session_info["workspacePath"]), "path": session_info["workspacePath"]} if session_info else None,
        "sessionInfo": session_info,
        "assistantRoot": ASSISTANT_ROOT,
        "capabilities": {
            "agent": False,
            "compile": True,
            "ingest": ["pdf", "step", "ifc"],
            "library": True,
            "approval": True,
            "sessionBinding": False,
        },
        "assistantUrl": DSH_URL,
        "libraryUrl": "http://127.0.0.1:5178",
        "defaultFamilyDir": "abutment_family",
        "feedback": [],
        "backend": "fastapi-dev",
    }


@app.get("/rcif-studio/api/sessions")
async def sessions():
    return {
        "ok": True,
        "sessions": recent_session_ids(),
        "defaultSessionId": "session-default",
        "activeSessionId": "session-default",
    }


@app.get("/rcif-studio/api/assistant")
async def assistant_info():
    return {
        "ok": True,
        "running": assistant_running(),
        "url": DSH_URL,
        "sessionId": "session-default",
        "launchable": True,
        "deepLinkSupported": False,
    }


@app.post("/rcif-studio/api/assistant/launch")
async def assistant_launch(request: Request):
    try:
        body = await request.json()
    except Exception:  # noqa: BLE001
        body = {}
    if assistant_running():
        return {"ok": True, "running": True, "launched": False, "url": DSH_URL}

    log_dir = os.path.join(ROOT_DIR, "rcif-tools", "out", "logs")
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, f"dsh-{DSH_PORT}.out.log")
    creationflags = getattr(subprocess, "DETACHED_PROCESS", 0) | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
    try:
        with open(log_path, "ab") as log:
            subprocess.Popen(
                ["cmd.exe", "/c", f"dsh --profile rcif-studio --port {DSH_PORT}"],
                cwd=ROOT_DIR,
                stdout=log,
                stderr=subprocess.STDOUT,
                creationflags=creationflags,
                close_fds=True,
            )
    except Exception as exc:  # noqa: BLE001
        return JSONResponse(status_code=500, content={"ok": False, "error": f"启动 DSH 失败: {exc}", "url": DSH_URL})

    for _ in range(24):
        await run_in_threadpool(time.sleep, 0.5)
        if assistant_running():
            return {"ok": True, "running": True, "launched": True, "url": DSH_URL, "log": log_path}
    return {"ok": True, "running": False, "launched": True, "url": DSH_URL, "log": log_path,
            "message": "DSH 已启动但端口尚未就绪，请稍后重试"}


@app.get("/rcif-studio/api/package")
async def package_info(path: str | None = None):
    target = path or CURRENT_PACKAGE
    if not target:
        return JSONResponse(status_code=404, content={"ok": False, "error": "尚未加载构件包"})
    try:
        meta = await run_in_threadpool(package_api.read_package, resolve_package(target))
    except Exception as exc:  # noqa: BLE001
        return JSONResponse(status_code=422, content={"ok": False, "error": f"构件包解析失败: {exc}"})
    meta["current"] = os.path.realpath(resolve_package(target)) == CURRENT_PACKAGE
    return meta


@app.get("/rcif-studio/api/package/preview.glb")
async def package_preview(path: str | None = None):
    target = path or CURRENT_PACKAGE
    if not target:
        return JSONResponse(status_code=404, content={"ok": False, "error": "尚未加载构件包"})
    try:
        pkg = resolve_package(target)
        meta = package_api.read_package(pkg)
        if not meta["preview"]["available"]:
            return JSONResponse(status_code=404, content={"ok": False, "error": "构件包不包含 preview.glb"})
        data = package_api.read_package_file(pkg, meta["preview"]["path"])
    except Exception as exc:  # noqa: BLE001
        return JSONResponse(status_code=404, content={"ok": False, "error": str(exc)})
    return Response(
        content=data,
        media_type="model/gltf-binary",
        headers={"Cache-Control": "no-store", "X-Content-Type-Options": "nosniff"},
    )


@app.get("/rcif-studio/api/package/source")
async def package_source(path: str | None = None):
    target = path or CURRENT_PACKAGE
    if not target:
        return JSONResponse(status_code=404, content={"ok": False, "error": "尚未加载构件包"})
    try:
        source = await run_in_threadpool(package_api.read_package_source, resolve_package(target))
    except Exception as exc:  # noqa: BLE001
        return JSONResponse(status_code=422, content={"ok": False, "error": str(exc)})
    source["packagePath"] = resolve_package(target)
    return source


@app.post("/rcif-studio/api/ingest")
async def ingest(request: Request):
    global CURRENT_PACKAGE
    try:
        body = await request.json()
    except Exception:  # noqa: BLE001
        return JSONResponse(status_code=400, content={"ok": False, "error": "Malformed JSON body"})

    kind = str(body.get("kind", "")).lower()
    if kind not in ("pdf", "step", "ifc"):
        return JSONResponse(status_code=422, content={"ok": False, "error": "kind 必须是 pdf/step/ifc"})
    if kind == "pdf":
        return JSONResponse(
            status_code=403,
            content={
                "ok": False,
                "code": "PDF_DIRECT_CONVERSION_DISABLED",
                "error": "PDF 不支持直接转换：PDF 只能由智能助手生成规则推断草稿，请改用“让智能助手转换”",
            },
        )
    try:
        source_path = resolve_in_root(str(body.get("sourcePath", "")))
    except ValueError as exc:
        return JSONResponse(status_code=403, content={"ok": False, "error": str(exc)})
    if not os.path.isfile(source_path):
        return JSONResponse(status_code=404, content={"ok": False, "error": f"源文件不存在: {source_path}"})

    family_id = str(body.get("familyId") or default_family_id(kind, source_path))
    revision = str(body.get("revision") or ("0.1.0" if kind == "pdf" else "1.0.0"))
    if not all(ch.isalnum() or ch == "_" for ch in family_id) or not family_id[0].isalpha():
        return JSONResponse(status_code=422, content={"ok": False, "error": "familyId 非法"})
    if len(revision.split(".")) != 3:
        return JSONResponse(status_code=422, content={"ok": False, "error": "revision 需为 x.y.z"})

    session_id = str(body.get("sessionId") or "session-default")
    session_segment = re.sub(r"[^A-Za-z0-9._-]", "_", session_id)[:80]
    session_info = read_session_meta(session_id)
    output_base = (session_info or {}).get("workspacePath") or os.path.join(ROOT_DIR, "rcif-tools", "out", "sessions", session_segment)
    output = body.get("outputPath") or os.path.join(output_base, "out", f"{family_id}_{revision}.rcif")
    try:
        output = resolve_in_root(str(output))
    except ValueError as exc:
        return JSONResponse(status_code=403, content={"ok": False, "error": str(exc)})
    os.makedirs(os.path.dirname(output), exist_ok=True)

    def convert() -> str:
        if kind == "step":
            import step2rcif

            pkg, _ = step2rcif.convert_step_to_rcif(
                input_step_path=source_path,
                output_dir=output,
                family_id=family_id,
                revision=revision,
            )
        elif kind == "ifc":
            import ifc2rcif

            pkg, _ = ifc2rcif.convert_ifc_to_rcif(source_path, output, family_id, revision)
        else:
            import pdf2rcif

            pkg, _ = pdf2rcif.convert_pdf_to_rcif(source_path, output, family_id, revision)
        return pkg

    try:
        pkg = await run_in_threadpool(convert)
    except Exception as exc:  # noqa: BLE001
        return {"ok": False, "error": f"转换失败: {exc}", "log": str(exc)}

    CURRENT_PACKAGE = os.path.realpath(pkg)
    if session_info:
        try:
            workspace_path = session_info["workspacePath"]
            write_session_meta(session_id, {
                "inputFile": os.path.relpath(source_path, workspace_path),
                "componentName": family_id,
                "outputPath": os.path.relpath(CURRENT_PACKAGE, workspace_path),
                "packagePath": CURRENT_PACKAGE,
                "sourceKind": kind,
                "revision": revision,
            })
        except Exception:  # noqa: BLE001
            pass
    try:
        meta = await run_in_threadpool(package_api.read_package, CURRENT_PACKAGE)
    except Exception as exc:  # noqa: BLE001
        return {"ok": False, "error": f"构件包已生成但解析失败: {exc}"}
    return {
        "ok": True,
        "packagePath": CURRENT_PACKAGE,
        "familyId": family_id,
        "revision": revision,
        "metadata": meta["metadata"],
        "log": f"[OK] 生成成功: {CURRENT_PACKAGE}",
    }


@app.post("/rcif-studio/api/generic/rebuild")
async def generic_rebuild_endpoint(request: Request):
    global CURRENT_PACKAGE
    try:
        body = await request.json()
    except Exception:  # noqa: BLE001
        return JSONResponse(status_code=400, content={"ok": False, "error": "Malformed JSON body"})
    session_id = str(body.get("sessionId") or "session-default")
    package_path = body.get("packagePath") or CURRENT_PACKAGE
    if not package_path:
        return JSONResponse(status_code=422, content={"ok": False, "error": "没有可重建的构件包"})
    try:
        package_path = resolve_package(str(package_path))
    except Exception as exc:  # noqa: BLE001
        return JSONResponse(status_code=422, content={"ok": False, "error": str(exc)})
    output = body.get("outputPath")
    if output:
        try:
            output = resolve_package(str(output))
        except Exception as exc:  # noqa: BLE001
            return JSONResponse(status_code=403, content={"ok": False, "error": str(exc)})
    try:
        import generic_rebuild

        pkg, info = await run_in_threadpool(
            generic_rebuild.rebuild,
            package_path,
            output,
            body.get("params") or {},
            body.get("generatorSource"),
            body.get("revision"),
            str(body.get("note") or "调试工作台重建"),
        )
    except Exception as exc:  # noqa: BLE001
        return JSONResponse(status_code=422, content={"ok": False, "error": str(exc)})
    CURRENT_PACKAGE = os.path.realpath(pkg)
    session_info = read_session_meta(session_id)
    if session_info:
        try:
            workspace_path = session_info["workspacePath"]
            write_session_meta(session_id, {
                "componentName": session_info.get("componentName") or os.path.basename(pkg),
                "familyId": os.path.splitext(os.path.basename(pkg))[0].rsplit("_", 1)[0],
                "outputPath": os.path.relpath(pkg, workspace_path),
                "packagePath": pkg,
                "revision": info.get("revision"),
                "sourceKind": "pdf",
            })
        except Exception:  # noqa: BLE001
            pass
    return {"ok": True, **info, "packagePath": pkg}


@app.post("/rcif-studio/api/command")
async def command(request: Request):
    try:
        body = await request.json()
    except Exception:  # noqa: BLE001
        return JSONResponse(status_code=400, content={"ok": False, "error": "Malformed JSON body"})
    text = str(body.get("text", "")).strip()
    if not text:
        return JSONResponse(status_code=422, content={"ok": False, "error": "指令内容为空"})
    return {
        "ok": True,
        "delivered": False,
        "deliveryReason": "FastAPI 调试后端没有 Agent 会话；请在 DSH 实例（3080）中使用工作台",
        "prompt": text,
    }


@app.post("/rcif-studio/api/feedback")
async def feedback(request: Request):
    try:
        body = await request.json()
    except Exception:  # noqa: BLE001
        return JSONResponse(status_code=400, content={"ok": False, "error": "Malformed JSON body"})
    annotations = body.get("annotations") or []
    return {
        "ok": True,
        "delivered": False,
        "deliveryReason": "FastAPI 调试后端不具备 Agent 会话注入能力；请在 DSH 实例（3080）中使用",
        "annotationCount": len(annotations),
        "screenshotPath": None,
    }


@app.post("/rcif-studio/api/library/ingest")
async def library_ingest(request: Request):
    try:
        body = await request.json()
    except Exception:  # noqa: BLE001
        return JSONResponse(status_code=400, content={"ok": False, "error": "Malformed JSON body"})
    target = body.get("packagePath") or CURRENT_PACKAGE
    if not target:
        return JSONResponse(status_code=404, content={"ok": False, "error": "尚未加载构件包"})
    try:
        pkg = resolve_package(target)
        meta = await run_in_threadpool(package_api.read_package, pkg)
    except Exception as exc:  # noqa: BLE001
        return JSONResponse(status_code=422, content={"ok": False, "error": str(exc)})

    stamp = time.strftime("%Y%m%d-%H%M%S")
    family = meta["metadata"].get("familyId") or "Imported"
    revision = meta["metadata"].get("revision") or "0.0.0"
    inbox = os.path.join(ROOT_DIR, "rcif-tools", "out", "library", f"{family}_{revision}_{stamp}")
    os.makedirs(inbox, exist_ok=True)
    shutil.copy2(pkg, os.path.join(inbox, os.path.basename(pkg)))
    prov = os.path.splitext(pkg)[0] + ".provenance.json"
    if os.path.isfile(prov):
        shutil.copy2(prov, os.path.join(inbox, os.path.basename(prov)))
    submission = {
        "schema": "rcif-library-submission/1",
        "submittedAt": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "packagePath": pkg,
        "packageSha256": meta["packageSha256"],
        "metadata": meta["metadata"],
        "note": str(body.get("note", ""))[:2000],
    }
    with open(os.path.join(inbox, "submission.json"), "w", encoding="utf-8") as fh:
        json.dump(submission, fh, ensure_ascii=False, indent=2)
    return {
        "ok": True,
        "mode": "inbox",
        "inboxPath": inbox,
        "libraryUrl": "http://127.0.0.1:5178",
        "message": "调试后端仅写入入库清单；启动 DSH 后由插件提交到构件库审核队列",
    }



# 握手路由
@app.get("/rcif-studio/api/init-token")
async def init_token(sessionId: str = "session-active"):
    return {
        "ok": True,
        "sessionId": sessionId,
        "capToken": f"cap-{uuid.uuid4()}",
        "pubkey": "dsh-host-ed25519-public-key-sample",
        "tenantId": "t_crdc",
        "userId": "u_bridge_01",
    }


# 预览几何（真实 preview.glb；与 DSH 插件的 /api/preview.glb 契约一致）
@app.get("/rcif-studio/api/preview.glb")
async def preview_glb(familyDir: str = "abutment_family"):
    try:
        family = resolve_in_root(familyDir)
    except ValueError as e:
        return JSONResponse(status_code=400, content={"ok": False, "error": str(e)})

    candidates = [
        os.path.join(family, "rcif", "display", "preview.glb"),
        os.path.join(family, "display", "preview.glb"),
        os.path.join(family, "out", "preview.glb"),
    ]
    for candidate in candidates:
        if os.path.isfile(candidate):
            return FileResponse(
                candidate,
                media_type="model/gltf-binary",
                headers={"Cache-Control": "no-store", "X-Content-Type-Options": "nosniff"},
            )
    return JSONResponse(
        status_code=404,
        content={"ok": False, "error": f"未找到 preview.glb 预览几何: {family}"},
    )


# 现场重跑门禁
@app.post("/rcif-studio/api/run-sandbox")
async def run_sandbox(request: Request):
    try:
        body = await request.json()
    except Exception:
        return JSONResponse(status_code=400, content={"error": "Malformed JSON body"})

    raw_target = body.get("targetDir", "abutment_family")
    try:
        target_path = resolve_in_root(raw_target)
    except ValueError as e:
        return JSONResponse(status_code=400, content={"error": str(e)})

    report = await run_in_threadpool(verify_sandbox.run_all_checks_in_memory, target_path)
    return {
        "ok": True,
        "passed": report.passed,
        "blocker_count": report.blocker_count,
        "major_count": report.major_count,
        "minor_count": report.minor_count,
        "duration_sec": round(report.duration_sec, 2),
        "table_truth_sha256": report.table_truth_sha256,
        "report": report.to_dict(),
    }


# 审批单数字签名 (明确 DEV 桩标明未上链状态，ISSUE-011)
@app.post("/rcif-studio/api/approve-control-grid")
async def approve_control_grid(request: Request):
    try:
        body = await request.json()
    except Exception:
        return JSONResponse(status_code=400, content={"error": "Malformed JSON body"})

    approval_id = f"appr-{uuid.uuid4().hex[:8]}"
    return {
        "ok": True,
        "approval": {
            "approvalId": approval_id,
            "sessionId": body.get("sessionId", "session-active"),
            "approvedParamsSha256": body.get("approvedParamsSha256", ""),
            "approvedTableTruthSha256": body.get("approvedTableTruthSha256", ""),
            "approvedSourceMode": body.get("approvedSourceMode", "parametric"),
            "approvedExactBrep": True,
            "approvedBy": body.get("approvedBy", "u_bridge_01"),
            "signedAt": time.strftime("%Y-%m-%dT%H:%M:%S+08:00"),
            "approvalAlgorithm": "Ed25519-DEV-MOCK",
            "approvalSignature": "DEV_UNSIGNED_STUB",
            "approvalVerified": False,
        },
    }


# 代码批注注入 (不落盘污染仓库，ISSUE-009)
@app.post("/rcif-studio/api/session-inject")
async def session_inject(request: Request):
    try:
        body = await request.json()
    except Exception:
        return JSONResponse(status_code=400, content={"error": "Malformed JSON body"})

    boundary = f"b_{uuid.uuid4().hex[:8]}"
    annotations = body.get("annotations", [])
    injected_text = (
        f"用户在 RCIF 工作台提交了代码审阅批注（包裹在 boundary=\"{boundary}\" 隔离块内）：\n"
        f"<user_code_annotation_data boundary=\"{boundary}\">\n"
        f"{json.dumps(annotations, ensure_ascii=False, indent=2)}\n"
        f"</user_code_annotation_data>"
    )

    return {
        "ok": True,
        "boundary": boundary,
        "annotationCount": len(annotations),
        "injectedPrompt": injected_text,
        "delivered": False,
        "deliveryReason": "FastAPI 调试后端不具备 Agent 会话注入能力；请在 DSH 宿主插件中使用该功能",
    }


# 打包提交
@app.post("/rcif-studio/api/compile-package")
async def compile_package(request: Request):
    try:
        body = await request.json()
    except Exception:
        return JSONResponse(status_code=400, content={"error": "Malformed JSON body"})

    raw_target = body.get("familyDir", "abutment_family")
    try:
        target_path = resolve_in_root(raw_target)
    except ValueError as e:
        return JSONResponse(status_code=400, content={"error": str(e)})

    out_dir = os.path.join(ROOT_DIR, "rcif-tools", "out")
    os.makedirs(out_dir, exist_ok=True)
    out_pkg = os.path.join(out_dir, f"{os.path.basename(target_path)}_1.0.0.rcif")

    try:
        pkg_path, pkg_sha, prov = pack_rcif_cli.pack_rcif_with_sandbox_and_provenance(target_path, out_pkg)
        global CURRENT_PACKAGE
        CURRENT_PACKAGE = os.path.realpath(pkg_path)
        return {
            "ok": True,
            "packagePath": pkg_path,
            "packageSha256": pkg_sha,
            "provenance": prov,
        }
    except Exception as exc:
        return JSONResponse(status_code=500, content={"error": f"打包失败: {exc}"})


# 静态工作台挂载 (静态文件防穿越)
workbench_dist = os.path.join(ROOT_DIR, "dsh-rcif", "workbench")
if os.path.exists(workbench_dist):
    app.mount("/rcif-studio/workbench", StaticFiles(directory=workbench_dist, html=True), name="workbench")

@app.get("/")
def root():
    return RedirectResponse(url="/rcif-studio/workbench/")


if __name__ == "__main__":
    import uvicorn
    port = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 3088
    uvicorn.run("workbench_server:app", host="127.0.0.1", port=port, log_level="info")
