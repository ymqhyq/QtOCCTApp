@echo off
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d D:\QtOCCTApp
cmake --build build --config Release --target test_load_bridge
pause
