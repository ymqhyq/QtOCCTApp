@echo off
set "ROOT_DIR=%~dp0"
set "OCCT_ROOT=%ROOT_DIR%3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined"
set "THIRDPARTY_DIR=%OCCT_ROOT%\3rdparty-vc14-64"
set "OCCT_DIR=%OCCT_ROOT%\opencascade-8.0.0-vc14-64"
set "QT_DIR=%ROOT_DIR%3rpart\QT6.7.0\msvc2019_64"

echo =======================================================
echo     brep_to_ifc Launcher (MSVC v142 Multi-Config)
echo =======================================================
echo  [1] Debug Configuration (Links *_d.lib / loads *_d.dll)
echo  [2] Release Configuration (Loads optimized Release DLLs)
echo =======================================================
set /p "choice=Please select build configuration to run [1 or 2]: "

if "%choice%"=="1" (
    set "CONFIG=Debug"
    set "OCCT_BIN_DIR=%OCCT_DIR%\win64\vc14\bind"
    set "IFC_BIN_DIR=%ROOT_DIR%3rpart\ifcopenshell-vs2026-x64-v142\bind"
    set "ASIACTIVE_BIN=%ROOT_DIR%3rpart\asiActiveData\build\Debug"
) else (
    set "CONFIG=Release"
    set "OCCT_BIN_DIR=%OCCT_DIR%\win64\vc14\bin"
    set "IFC_BIN_DIR=%ROOT_DIR%3rpart\ifcopenshell-vs2026-x64-v142\bin"
    set "ASIACTIVE_BIN=%ROOT_DIR%3rpart\asiActiveData\build\Release"
)

echo --- Setting Environment Variables for %CONFIG% ---
set "QT_BIN=%QT_DIR%\bin"
set "QT_PLUGIN_PATH=%QT_DIR%\plugins"

set "TP=%THIRDPARTY_DIR%"
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

set "APP_BIN=%ROOT_DIR%build_v142\bin\%CONFIG%"
set "CORE_BIN=%ROOT_DIR%build_v142\core-data-model\%CONFIG%"

set "PATH=%QT_BIN%;%OCCT_BIN_DIR%;%ANGLE%;%DRACO%;%FFMPEG%;%FREEIMAGE%;%FREETYPE%;%GL2PS%;%JEMALLOC%;%LZMA%;%OPENVR%;%TBB%;%TCLTK%;%VTK%;%ZLIB%;%IFC_BIN_DIR%;%ASIACTIVE_BIN%;%CORE_BIN%;%APP_BIN%;%PATH%"

echo --- Launching brep_to_ifc.exe (%CONFIG%) ---
if not exist "%APP_BIN%\brep_to_ifc.exe" (
    echo.
    echo [ERROR] Cannot find "%APP_BIN%\brep_to_ifc.exe"!
    echo Please compile the 'brep_to_ifc' target in Visual Studio in "%CONFIG%" mode first.
    echo.
    pause
    exit /b 1
)

"%APP_BIN%\brep_to_ifc.exe"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Application exited with error code: %ERRORLEVEL%
    pause
)
