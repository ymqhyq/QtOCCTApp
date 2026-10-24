@echo off
chcp 65001 >nul
setlocal

set "ROOT=%~dp0"
set "APP=%ROOT%rcif-web"

if not exist "%APP%\package.json" (
  echo [ERROR] rcif-web not found: %APP%
  pause
  exit /b 1
)

cd /d "%APP%"

where npm >nul 2>nul
if errorlevel 1 (
  echo [ERROR] npm is not installed or not in PATH.
  pause
  exit /b 1
)

if not exist "node_modules" (
  echo [INFO] Installing dependencies...
  call npm install
  if errorlevel 1 (
    echo [ERROR] npm install failed.
    pause
    exit /b 1
  )
)

if exist "%APP%\.venv-cq\Scripts\python.exe" (
  set "CQ_PYTHON=%APP%\.venv-cq\Scripts\python.exe"
  echo [INFO] CadQuery: %CQ_PYTHON%
) else (
  echo [WARN] CadQuery venv not found; using fallback modeler.
)

echo.
echo [INFO] Starting RCIF Component Library...
echo [INFO] URL: http://127.0.0.1:5178
echo [INFO] Press Ctrl+C to stop.
echo.

start "" cmd /c "timeout /t 3 >nul & start http://127.0.0.1:5178"
call npm run dev

if errorlevel 1 (
  echo.
  echo [ERROR] Server exited with an error.
  pause
)

endlocal
