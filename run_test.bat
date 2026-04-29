@echo off
setlocal

set QT_BIN=D:\Qt\6.10.1\msvc2022_64\bin
set OCCT_BIN=D:\QtOCCTApp\3rpart\opencascade-7.9.3-vc14-64\win64\vc14\bin
set OCCT_3RD_FREETYE=D:\QtOCCTApp\3rpart\3rdparty-vc14-64\freetype-2.13.3-x64\bin
set OCCT_3RD_TBB=D:\QtOCCTApp\3rpart\3rdparty-vc14-64\tbb-2021.13.0-x64\bin
set OCCT_3RD_FREEIMAGE=D:\QtOCCTApp\3rpart\3rdparty-vc14-64\freeimage-3.18.0-x64\bin
set OCCT_3RD_LZMA=D:\QtOCCTApp\3rpart\3rdparty-vc14-64\lzma-5.2.2-vc14-64\bin
set OCCT_3RD_ZLIB=D:\QtOCCTApp\3rpart\3rdparty-vc14-64\zlib-1.2.8-vc14-64\bin
set OCCT_3RD_TCLTK=D:\QtOCCTApp\3rpart\3rdparty-vc14-64\tcltk-8.6.15-x64\bin
set OCCT_3RD_OPENVR=D:\QtOCCTApp\3rpart\3rdparty-vc14-64\openvr-1.14.15-64\bin\win64
set OCCT_3RD_FFMPEG=D:\QtOCCTApp\3rpart\3rdparty-vc14-64\ffmpeg-3.3.4-64\bin
set VTK_BIN=D:\QtOCCTApp\3rpart\3rdparty-vc14-64\vtk-9.4.1-x64\bin
set ACTDATA_BIN=D:\QtOCCTApp\build\3rpart\asiActiveData
set TEST_BIN=D:\QtOCCTApp\build\bin
set MSVC_REDIST=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT

set PATH=%QT_BIN%;%OCCT_BIN%;%OCCT_3RD_FREETYE%;%OCCT_3RD_TBB%;%OCCT_3RD_FREEIMAGE%;%OCCT_3RD_LZMA%;%OCCT_3RD_ZLIB%;%OCCT_3RD_TCLTK%;%OCCT_3RD_OPENVR%;%OCCT_3RD_FFMPEG%;%VTK_BIN%;%ACTDATA_BIN%;%TEST_BIN%;%MSVC_REDIST%;%PATH%

echo Current PATH: %PATH%
echo Running test_load_bridge...
cd /d D:\QtOCCTApp\core-data-model
"%TEST_BIN%\test_load_bridge.exe"
pause
endlocal
