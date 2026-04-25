@echo off
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist build_win rd /s /q build_win
mkdir build_win
cd build_win
cmake .. -G "Visual Studio 18 2026" -A x64
cmake --build . --config Release
