@echo off
setlocal

set QT_BIN=D:\QtOCCTApp\3rpart\QT6.7.0\msvc2019_64\bin
set OCCT_BIN=D:\QtOCCTApp\3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined\opencascade-8.0.0-vc14-64\win64\vc14\bin
set TP=D:\QtOCCTApp\3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined\3rdparty-vc14-64

set PATH=%QT_BIN%;%OCCT_BIN%;%TP%\angle-gles2-2.1.0-vc14-64\bin;%TP%\draco-1.4.1-vc14-64\bin;%TP%\ffmpeg-3.3.4-64\bin;%TP%\freeimage-3.18.0-x64\bin;%TP%\freetype-2.13.3-x64\bin;%TP%\gl2ps-1.3.8-vc14-64\bin;%TP%\jemalloc-vc14-64\bin;%TP%\lzma-5.2.2-vc14-64\bin;%TP%\openvr-1.14.15-64\bin\win64;%TP%\tbb-2021.13.0-x64\bin;%TP%\tcltk-8.6.15-x64\bin;%TP%\vtk-9.4.1-x64\bin;%TP%\zlib-1.2.8-vc14-64\bin;D:\QtOCCTApp\build_v142\bin\Release;D:\QtOCCTApp\build_v142\3rpart\asiActiveData\Release;%PATH%

cd /d "D:\QtOCCTApp\core-data-model"
echo Running test_load_bridge...
"D:\QtOCCTApp\build_v142\bin\Release\test_load_bridge.exe"

echo.
echo Finished with exit code %ERRORLEVEL%
pause
endlocal