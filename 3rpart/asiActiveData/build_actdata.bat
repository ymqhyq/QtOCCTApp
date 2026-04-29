@echo off
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d %~dp0
if not exist build mkdir build
cd build
cmake .. -DOpenCASCADE_DIR=D:\QtOCCTApp\3rpart\opencascade-7.9.3-vc14-64\cmake -DVTK_DIR=D:\QtOCCTApp\3rpart\3rdparty-vc14-64\vtk-9.4.1-x64\lib\cmake\vtk-9.4 -DTBB_DIR=D:\QtOCCTApp\3rpart\3rdparty-vc14-64\tbb-2021.13.0-x64\lib\cmake\TBB
cmake --build . --config Debug
pause
