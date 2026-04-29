@echo off
:: Default configuration is Release, can be overridden by first argument
set "CONFIG=Release"
if /i "%1"=="debug" set "CONFIG=Debug"

set "THIRDPARTY_DIR=D:\QtOCCTApp\3rpart\3rdparty-vc14-64"
set "OCCT_DIR=D:\QtOCCTApp\3rpart\opencascade-7.9.3-vc14-64"
set "ACTIVE_DATA_BIN=%~dp0build\%CONFIG%"

:: Add ActiveData bin to PATH
set "PATH=%ACTIVE_DATA_BIN%;%PATH%"

:: Add OCCT bin to PATH
set "PATH=%OCCT_DIR%\win64\vc14\bin;%PATH%"

:: Add 3rdparty bins to PATH
set "PATH=%THIRDPARTY_DIR%\vtk-9.4.1-x64\bin;%PATH%"
set "PATH=%THIRDPARTY_DIR%\tbb-2021.13.0-x64\bin;%PATH%"
set "PATH=%THIRDPARTY_DIR%\freetype-2.13.3-x64\bin;%PATH%"
set "PATH=%THIRDPARTY_DIR%\freeimage-3.18.0-x64\bin;%PATH%"
set "PATH=%THIRDPARTY_DIR%\tcltk-8.6.15-x64\bin;%PATH%"
set "PATH=%THIRDPARTY_DIR%\zlib-1.2.8-vc14-64\bin;%PATH%"

echo [INFO] ActiveData (%CONFIG%) environment initialized.
echo [INFO] Path to DLL: %ACTIVE_DATA_BIN%\asiActiveData.dll
