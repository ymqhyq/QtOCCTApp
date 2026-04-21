@echo off
setlocal

set MY_PYTHON_EXE=D:\ProgramData\miniforge3\envs\cq\python.exe
set CQ_BIN=D:\ProgramData\miniforge3\envs\cq
set CQ_LIB_BIN=D:\ProgramData\miniforge3\envs\cq\Library\bin
set CQ_SCRIPTS=D:\ProgramData\miniforge3\envs\cq\Scripts

set PATH=%CQ_BIN%;%CQ_LIB_BIN%;%CQ_SCRIPTS%;%PATH%

echo ========================================
echo   OCCT Modeling Microservice Cluster
echo   Starting 4 instances via Dapr
echo ========================================
echo.

REM Start 4 instances with same app-id 'modeling-service' for load balancing
start "Dapr Service 1" dapr run --app-id modeling-service --app-port 8000 --dapr-http-port 3500 --max-body-size 32Mi -- "%MY_PYTHON_EXE%" -m uvicorn scripts-service.main:app --port 8000
start "Dapr Service 2" dapr run --app-id modeling-service --app-port 8002 --dapr-http-port 3502 --max-body-size 32Mi -- "%MY_PYTHON_EXE%" -m uvicoRunnirn scripts-service.main:app --port 8002
start "Dapr Service 3" dapr run --app-id modeling-service --app-port 8003 --dapr-http-port 3503 --max-body-size 32Mi -- "%MY_PYTHON_EXE%" -m uvicorn scripts-service.main:app --port 8003
start "Dapr Service 4" dapr run --app-id modeling-service --app-port 8004 --dapr-http-port 3504 --max-body-size 32Mi -- "%MY_PYTHON_EXE%" -m uvicorn scripts-service.main:app --port 8004

echo Cluster started!
echo You can use C++ Qt frontend to access the Dapr Sidecar at any of the ports (3500, 3502, 3503, 3504).
echo Because they share the same app-id 'modeling-service', requests will be load-balanced across all instances.
pause
endlocal
