@echo off
setlocal

set MY_PYTHON_EXE=D:\ProgramData\miniforge3\envs\cq\python.exe
set CQ_BIN=D:\ProgramData\miniforge3\envs\cq
set CQ_LIB_BIN=D:\ProgramData\miniforge3\envs\cq\Library\bin
set CQ_SCRIPTS=D:\ProgramData\miniforge3\envs\cq\Scripts

set "ROOT_DIR=D:\QtOCCTApp\"
set "OCCT_ROOT=D:\QtOCCTApp\3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined"
set "THIRDPARTY_DIR=%OCCT_ROOT%\3rdparty-vc14-64"
set "OCCT_DIR=%OCCT_ROOT%\opencascade-8.0.0-vc14-64"
set "QT_DIR=%ROOT_DIR%3rpart\QT6.7.0\msvc2019_64"

set "QT_BIN=%QT_DIR%\bin"
set "OCCT_BIN=%OCCT_DIR%\win64\vc14\bin"
set "OCCT_BIND=%OCCT_DIR%\win64\vc14\bind"

set "ANGLE=%THIRDPARTY_DIR%\angle-gles2-2.1.0-vc14-64\bin"
set "DRACO=%THIRDPARTY_DIR%\draco-1.4.1-vc14-64\bin"
set "FFMPEG=%THIRDPARTY_DIR%\ffmpeg-3.3.4-64\bin"
set "FREEIMAGE=%THIRDPARTY_DIR%\freeimage-3.18.0-x64\bin"
set "FREETYPE=%THIRDPARTY_DIR%\freetype-2.13.3-x64\bin"
set "GL2PS=%THIRDPARTY_DIR%\gl2ps-1.3.8-vc14-64\bin"
set "JEMALLOC=%THIRDPARTY_DIR%\jemalloc-vc14-64\bin"
set "LZMA=%THIRDPARTY_DIR%\lzma-5.2.2-vc14-64\bin"
set "OPENVR=%THIRDPARTY_DIR%\openvr-1.14.15-64\bin\win64"
set "TBB=%THIRDPARTY_DIR%\tbb-2021.13.0-x64\bin"
set "TCLTK=%THIRDPARTY_DIR%\tcltk-8.6.15-x64\bin"
set "VTK=%THIRDPARTY_DIR%\vtk-9.4.1-x64\bin"
set "ZLIB=%THIRDPARTY_DIR%\zlib-1.2.8-vc14-64\bin"

set "IFC_BIN=D:\QtOCCTApp\3rpart\ifcopenshell-vs2026-x64-v142\bin"
set "APP_BIN=D:\QtOCCTApp\build_v142\Release"
set "APP_BIN2=D:\QtOCCTApp\build_v142\bin\Release"
set "ACTDATA_BIN=D:\QtOCCTApp\3rpart\asiActiveData\build\Release"

set PATH=%CQ_BIN%;%CQ_LIB_BIN%;%CQ_SCRIPTS%;%QT_BIN%;%OCCT_BIN%;%OCCT_BIND%;%ANGLE%;%DRACO%;%FFMPEG%;%FREEIMAGE%;%FREETYPE%;%GL2PS%;%JEMALLOC%;%LZMA%;%OPENVR%;%TBB%;%TCLTK%;%VTK%;%ZLIB%;%IFC_BIN%;%APP_BIN%;%APP_BIN2%;%ACTDATA_BIN%;%PATH%

echo ========================================
echo   OCCT Modeling Microservice (No Dapr)
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
