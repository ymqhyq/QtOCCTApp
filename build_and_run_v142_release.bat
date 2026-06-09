@echo off
set "ROOT_DIR=D:\QtOCCTApp\"
set "OCCT_ROOT=D:\QtOCCTApp\3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined"
set "THIRDPARTY_DIR=%OCCT_ROOT%\3rdparty-vc14-64"
set "OCCT_DIR=%OCCT_ROOT%\opencascade-8.0.0-vc14-64"
set "QT_DIR=%ROOT_DIR%3rpart\QT6.7.0\msvc2019_64"

echo --- Compiling QtOCCTApp in v142 RELEASE mode ---
cmake --build D:\QtOCCTApp\build_v142 --config Release --parallel 8
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo --- Setting Environment Variables ---
set "QT_BIN=%QT_DIR%\bin"
set "QT_PLUGIN_PATH=%QT_DIR%\plugins"
set "OCCT_BIN=D:\QtOCCTApp\3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined\opencascade-8.0.0-vc14-64\win64\vc14\bin"
set "OCCT_BIND=D:\QtOCCTApp\3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined\opencascade-8.0.0-vc14-64\win64\vc14\bind"

set "TP=D:\QtOCCTApp\3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined\3rdparty-vc14-64"
set "ANGLE=%TP%\angle-gles2-2.1.0-vc14-64\bin"
set "DRACO=%TP%\draco-1.4.1-vc14-64\bin"
set "FFMPEG=%TP%\ffmpeg-3.3.4-64\bin"
set "FREEIMAGE=%TP%\freeimage-3.18.0-x64\bin"
set "FREETYPE=%TP%\freetype-2.13.3-x64\bin"
set "GL2PS=%TP%\gl2ps-1.3.8-vc14-64\bin"
set "JEMALLOC=%TP%\jemalloc-vc14-64\bin"
set "LZMA=%TP%\lzma-5.2.2-vc14-64\bin"
set "OPENVR=%TP%\openvr-1.14.15-64\bin\win64"
set "TBB=%TP%\tbb-2021.13.0-x64\bin"
set "TCLTK=%TP%\tcltk-8.6.15-x64\bin"
set "VTK=%TP%\vtk-9.4.1-x64\bin"
set "ZLIB=%TP%\zlib-1.2.8-vc14-64\bin"

set "IFC_BIN=D:\QtOCCTApp\3rpart\ifcopenshell-vs2026-x64-v142\bin"
set "RIBBON_BIN=D:\QtOCCTApp\3rpart\SARibbon_qt6.7.0_MSVC_x64_v142\bin"
set "APP_BIN=D:\QtOCCTApp\build_v142\Release"
set "APP_BIN2=D:\QtOCCTApp\build_v142\bin\Release"
set "APP_BIN3=D:\QtOCCTApp\build_v142\libs\shxparser\Release"
set "APP_BIN4=D:\QtOCCTApp\3rpart\asiActiveData\build\Release"

set "PATH=%QT_BIN%;%OCCT_BIN%;%OCCT_BIND%;%ANGLE%;%DRACO%;%FFMPEG%;%FREEIMAGE%;%FREETYPE%;%GL2PS%;%JEMALLOC%;%LZMA%;%OPENVR%;%TBB%;%TCLTK%;%VTK%;%ZLIB%;%IFC_BIN%;%RIBBON_BIN%;%APP_BIN%;%APP_BIN2%;%APP_BIN3%;%APP_BIN4%;%PATH%"

echo Starting QtOCCTApp (v142 Release)...
"%APP_BIN%\QtOCCTApp.exe"
if %ERRORLEVEL% NEQ 0 (
    echo Application exited with error code: %ERRORLEVEL%
    pause
)
