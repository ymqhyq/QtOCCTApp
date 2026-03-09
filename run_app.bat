@echo off
setlocal

set QT_BIN=D:\Qt\6.10.1\msvc2022_64\bin
set OCCT_BIN=D:\GitHub_Ymqhyq\OCCT\build-vs2022-x64\win64\vc14\bin
set OCCT_3RD_FREETYE=D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\freetype-2.13.3-x64\bin
set OCCT_3RD_TBB=D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\tbb-2021.13.0-x64\bin
set OCCT_3RD_FREEIMAGE=D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\freeimage-3.18.0-x64\bin
set OCCT_3RD_LZMA=D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\lzma-5.2.2-vc14-64\bin
set OCCT_3RD_ZLIB=D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\zlib-1.2.8-vc14-64\bin
set OCCT_3RD_TCLTK=D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\tcltk-8.6.15-x64\bin
set OCCT_3RD_OPENVR=D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\openvr-1.14.15-64\bin\win64
set OCCT_3RD_FFMPEG=D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\ffmpeg-3.3.4-64\bin
set MSVC_REDIST=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT
set QT_PLUGIN_PATH=D:\Qt\6.10.1\msvc2022_64\plugins
set QT_QPA_PLATFORM_PLUGIN_PATH=%QT_PLUGIN_PATH%\platforms

set MY_PYTHON_EXE=D:\ProgramData\miniforge3\envs\cq\python.exe
set CQ_BIN=D:\ProgramData\miniforge3\envs\cq
set CQ_LIB_BIN=D:\ProgramData\miniforge3\envs\cq\Library\bin
set CQ_SCRIPTS=D:\ProgramData\miniforge3\envs\cq\Scripts

set PATH=%QT_BIN%;%OCCT_BIN%;%OCCT_3RD_FREETYE%;%OCCT_3RD_TBB%;%OCCT_3RD_FREEIMAGE%;%OCCT_3RD_LZMA%;%OCCT_3RD_ZLIB%;%OCCT_3RD_TCLTK%;%OCCT_3RD_OPENVR%;%OCCT_3RD_FFMPEG%;%MSVC_REDIST%;%CQ_BIN%;%CQ_LIB_BIN%;%CQ_SCRIPTS%;%PATH%

echo Environment configured. > run_debug.log
echo Starting QtOCCTApp...
"d:\QtOCCTApp\build2\Debug\QtOCCTApp.exe"
if %ERRORLEVEL% NEQ 0 (
    echo Application exited with error code: %ERRORLEVEL%
    pause
)

endlocal