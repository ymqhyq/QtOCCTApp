@echo off
set "ROOT_DIR=D:\QtOCCTApp\"
set "OCCT_ROOT=D:\QtOCCTApp\3rpart\opencascade-8.0.0-vc14-64-pch-with-debug-combined"
set "THIRDPARTY_DIR=%OCCT_ROOT%\3rdparty-vc14-64"
set "OCCT_DIR=%OCCT_ROOT%\opencascade-8.0.0-vc14-64"
set "QT_DIR=%ROOT_DIR%3rpart\QT6.7.0\msvc2019_64"

set "PATH=%OCCT_DIR%\win64\vc14\bin;%THIRDPARTY_DIR%\vtk-9.4.1-x64\bin;%THIRDPARTY_DIR%\tbb-2021.13.0-x64\bin;%THIRDPARTY_DIR%\freetype-2.13.3-x64\bin;%THIRDPARTY_DIR%\zlib-1.2.8-vc14-64\bin;%THIRDPARTY_DIR%\freeimage-3.18.0-x64\bin;%THIRDPARTY_DIR%\tcltk-8.6.15-x64\bin;%THIRDPARTY_DIR%\angle-gles2-2.1.0-vc14-64\bin;%THIRDPARTY_DIR%\ffmpeg-3.3.4-64\bin;%QT_DIR%\bin;D:\QtOCCTApp\build\bin\Release;D:\QtOCCTApp\build\3rpart\asiActiveData\Release;D:\QtOCCTApp\build\Release;%PATH%"

echo Starting QtOCCTApp (Release)...
"D:\QtOCCTApp\build\Release\QtOCCTApp.exe"