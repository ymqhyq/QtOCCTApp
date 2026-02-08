@echo off
echo Starting QtOCCTApp...
set PATH=%PATH%;D:\GitHub_Ymqhyq\OCCT\build-vs2022-x64\win64\vc14\bin
set PATH=%PATH%;D:\Qt\6.10.1\msvc2022_64\bin
set PATH=%PATH%;C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT

REM Add 3rdparty DLLs
set PATH=%PATH%;D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\freetype-2.13.3-x64\bin
set PATH=%PATH%;D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\tbb-2021.13.0-x64\bin
set PATH=%PATH%;D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\freeimage-3.18.0-x64\bin

echo PATH is set.
cd /d "D:\QwenCodeWS\build_temp\Release"
QtOCCTApp.exe
if %ERRORLEVEL% NEQ 0 (
    echo Application failed with error code: %ERRORLEVEL%
    pause
)