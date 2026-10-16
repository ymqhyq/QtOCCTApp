@echo off
setlocal

set MY_PYTHON_EXE=D:\ProgramData\miniforge3\envs\cq\python.exe
set CQ_BIN=D:\ProgramData\miniforge3\envs\cq
set CQ_LIB_BIN=D:\ProgramData\miniforge3\envs\cq\Library\bin
set CQ_SCRIPTS=D:\ProgramData\miniforge3\envs\cq\Scripts

set PATH=%CQ_BIN%;%CQ_LIB_BIN%;%CQ_SCRIPTS%;%PATH%

echo Running test_export.py...
"%MY_PYTHON_EXE%" "%~dp0test_export.py"

if %ERRORLEVEL% NEQ 0 (
    echo Error occurred.
) else (
    echo Successfully finished.
)

endlocal
