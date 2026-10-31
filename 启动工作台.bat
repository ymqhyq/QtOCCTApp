@echo off
setlocal
cd /d "%~dp0"
title RCIF 铁路参数化构件工作台

echo ================================================================
echo   铁路参数化智能构件构建平台 (RCIF Studio Workbench)
echo   服务地址: http://127.0.0.1:3088/
echo ================================================================
echo   [旧后端] 3088 仅用于离线调试；正式使用请运行 启动助手.bat（3090）
echo ================================================================
echo.

set "PYTHON_EXE=D:\ProgramData\miniforge3\envs\cq\python.exe"

if exist "%PYTHON_EXE%" goto :python_found
echo [错误] 未找到 Python 解释器: %PYTHON_EXE%
echo 请检查 Miniforge 环境路径是否正确。
pause
exit /b 1

:python_found
rem 检查 3088 端口是否已在运行
netstat -ano | findstr ":3088" >nul 2>&1
if %ERRORLEVEL% EQU 0 goto :service_already_running

echo [*] 正在启动 RCIF 工作台后台服务 [端口 3088]...
start "RCIF_Workbench_Server" "%PYTHON_EXE%" "%~dp0rcif-tools\workbench_server.py" 3088
timeout /t 2 /nobreak >nul
goto :open_browser

:service_already_running
echo [*] 检测到后台服务已在运行 [端口 3088]。

:open_browser
echo [*] 正在打开默认浏览器访问 RCIF 工作台...
start "" "http://127.0.0.1:3088/"

echo.
echo ================================================================
echo   RCIF 构件工作台已就绪!
echo   - 浏览器地址: http://127.0.0.1:3088/
echo   - 核心功能: 3D拓扑点选、代码行级批注、6道门禁复算、方量残差分析
echo ================================================================
echo.
echo 提示: 构件后台服务已常驻运行，你可以安全关闭本窗口。
echo.
pause
endlocal
