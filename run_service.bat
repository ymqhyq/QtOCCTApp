@echo off
setlocal

set MY_PYTHON_EXE=C:\Users\yangx\miniforge3\envs\cq\python.exe
set CQ_BIN=C:\Users\yangx\miniforge3\envs\cq
set CQ_LIB_BIN=C:\Users\yangx\miniforge3\envs\cq\Library\bin
set CQ_SCRIPTS=C:\Users\yangx\miniforge3\envs\cq\Scripts

set PATH=%CQ_BIN%;%CQ_LIB_BIN%;%CQ_SCRIPTS%;%PATH%

echo ========================================
echo   OCCT Modeling Microservice
echo   http://127.0.0.1:8000
echo ========================================
echo.
echo Starting service...
"%MY_PYTHON_EXE%" "%~dp0scripts-service\main.py"

if %ERRORLEVEL% NEQ 0 (
    echo Service exited with error code: %ERRORLEVEL%
    pause
)

endlocal
