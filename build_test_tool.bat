@echo off
setlocal

echo === BRep-to-IFC Test Tool Builder (VS2026 + v142 Toolset) ===

:: Clean
if exist build\test (
    echo --- Cleaning previous test build ---
    rd /s /q build\test
)

:: Setup VS2026 Environment with v142 Toolset
echo --- Setting up VS2026 Environment with v142 Toolset ---
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.26100.0 -vcvars_ver=14.2

:: Manually inject SDK paths
set "SDK_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0"
set "LIB=%LIB%;%SDK_LIB%\um\x64;%SDK_LIB%\ucrt\x64"
set "SDK_INC=C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0"
set "INCLUDE=%INCLUDE%;%SDK_INC%\um;%SDK_INC%\ucrt;%SDK_INC%\shared"

:: Configure (test as standalone project)
echo --- Configuring CMake ---
cmake -B build\test -S test -G "Ninja" -DCMAKE_BUILD_TYPE=Release

:: Build
echo --- Building ---
cmake --build build\test --config Release

if %ERRORLEVEL% EQU 0 (
    echo.
    echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    echo BUILD SUCCESSFUL!
    echo Executable: D:\QtOCCTApp\build\test\brep_to_ifc.exe
    echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
) else (
    echo.
    echo -------------------------------------------------------
    echo BUILD FAILED.
    echo -------------------------------------------------------
)

endlocal
