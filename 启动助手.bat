@echo off
setlocal
cd /d "%~dp0"
title DeepSeek Harness 智能构件助手 [3090]

echo ================================================================
echo   DeepSeek Harness 智能构件助手 [RCIF Studio]
echo   助手地址: http://127.0.0.1:3090/
echo   工作台:   http://127.0.0.1:3090/rcif-studio/workbench/
echo   安全模式: 完全隔离 [TELEMETRY DISABLED]
echo ================================================================
echo.

rem 安全环境变量与工具链
set "PYTHONIOENCODING=utf-8"
set "PATH=D:\ProgramData\miniforge3\envs\cq;D:\ProgramData\miniforge3\envs\cq\Scripts;%PATH%"
set "DSH_TELEMETRY_DISABLED=1"
set "DSH_TELEMETRY_MODE=DISABLED"
set "DSH_TELEMETRY_OTLP_URL="
set "DSH_PRODUCT_ANALYTICS_OTLP_URL="

rem 3090 已在运行：不重复启动，直接打开工作台
netstat -ano | findstr ":3090" | findstr /C:"LISTENING" >nul 2>&1
if %ERRORLEVEL% EQU 0 (
  echo [*] 检测到 3090 已在运行，直接打开工作台。
  echo [*] 若工作台显示 401，请关闭当前 DSH 后重新运行本脚本以获取新的登录令牌。
  start "" "http://127.0.0.1:3090/rcif-studio/workbench/"
  exit /b 0
)

echo [*] 工作区路径: %CD%
echo [*] 正在启动 DeepSeek Harness [3090]...
echo.

rem DSH 会自行打开带令牌的助手页面；这里轮询端口，就绪后 2 秒再打开工作台
start "" powershell -NoProfile -WindowStyle Hidden -Command "for($i=0;$i -lt 240;$i++){ $c=New-Object Net.Sockets.TcpClient; try { $c.Connect('127.0.0.1',3090); $c.Close(); break } catch { Start-Sleep -Milliseconds 250 } }; Start-Sleep -Seconds 3; Start-Process 'http://127.0.0.1:3090/rcif-studio/workbench/'"

call dsh --profile rcif-studio --port 3090 %*

if %ERRORLEVEL% NEQ 0 (
  echo.
  echo [提示] DSH 已退出 [代码: %ERRORLEVEL%]
  pause
)
endlocal
