@echo off
setlocal

set QT_BIN=D:\Qt\6.10.1\msvc2022_64\bin
set OCCT_BIN=D:\QtOCCTApp\3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined\opencascade-8.0.0-vc14-64\win64\vc14\bin;D:\QtOCCTApp\3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined\opencascade-8.0.0-vc14-64\win64\vc14\bind

set TP=D:\QtOCCTApp\3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined\3rdparty-vc14-64
set ANGLE=%TP%\angle-gles2-2.1.0-vc14-64\bin
set DRACO=%TP%\draco-1.4.1-vc14-64\bin
set FFMPEG=%TP%\ffmpeg-3.3.4-64\bin
set FREEIMAGE=%TP%\freeimage-3.18.0-x64\bin
set FREETYPE=%TP%\freetype-2.13.3-x64\bin
set GL2PS=%TP%\gl2ps-1.3.8-vc14-64\bin
set JEMALLOC=%TP%\jemalloc-vc14-64\bin
set LZMA=%TP%\lzma-5.2.2-vc14-64\bin
set OPENVR=%TP%\openvr-1.14.15-64\bin\win64
set TBB=%TP%\tbb-2021.13.0-x64\bin
set TCLTK=%TP%\tcltk-8.6.15-x64\bin
set VTK=%TP%\vtk-9.4.1-x64\bin
set ZLIB=%TP%\zlib-1.2.8-vc14-64\bin

set APP_BIN=D:\QtOCCTApp\build\Debug
set APP_BIN2=D:\QtOCCTApp\build\bin
set APP_BIN3=D:\QtOCCTApp\build\libs\shxparser
set APP_BIN4=D:\QtOCCTApp\build\3rpart\asiActiveData
set QT_PLUGIN_PATH=D:\Qt\6.10.1\msvc2022_64\plugins

set PATH=%QT_BIN%;%OCCT_BIN%;%ANGLE%;%DRACO%;%FFMPEG%;%FREEIMAGE%;%FREETYPE%;%GL2PS%;%JEMALLOC%;%LZMA%;%OPENVR%;%TBB%;%TCLTK%;%VTK%;%ZLIB%;%APP_BIN%;%APP_BIN2%;%APP_BIN3%;%APP_BIN4%;%PATH%

echo Environment configured.
echo Starting QtOCCTApp...
"D:\QtOCCTApp\build\Debug\QtOCCTApp.exe"
if %ERRORLEVEL% NEQ 0 (
    echo Application exited with error code: %ERRORLEVEL%
    pause
)

endlocal