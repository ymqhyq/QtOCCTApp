@echo off
setlocal EnableDelayedExpansion
echo Starting QtOCCTApp with CadQuery Integration...

REM --- Attempt to find Miniforge/Anaconda Root ---
set MINIFORGE_ROOT=""
if exist "%USERPROFILE%\miniforge3\Scripts\activate.bat" (
    set MINIFORGE_ROOT=%USERPROFILE%\miniforge3
) else if exist "C:\ProgramData\miniforge3\Scripts\activate.bat" (
    set MINIFORGE_ROOT=C:\ProgramData\miniforge3
) else if exist "C:\miniforge3\Scripts\activate.bat" (
    set MINIFORGE_ROOT=C:\miniforge3
) else if exist "%USERPROFILE%\anaconda3\Scripts\activate.bat" (
    set MINIFORGE_ROOT=%USERPROFILE%\anaconda3
) else if exist "C:\ProgramData\anaconda3\Scripts\activate.bat" (
    set MINIFORGE_ROOT=C:\ProgramData\anaconda3
)

set MY_PYTHON_EXE=""

if not "%MINIFORGE_ROOT%"=="" (
    echo Found Conda Root: !MINIFORGE_ROOT!
    REM Check specifically for 'cq' environment python
    set PROPOSED_PYTHON="!MINIFORGE_ROOT!\envs\cq\python.exe"
    
    if exist !PROPOSED_PYTHON! (
        echo Found 'cq' environment python: !PROPOSED_PYTHON!
        set MY_PYTHON_EXE=!PROPOSED_PYTHON!
    ) else (
        echo WARNING: Could not find python in 'cq' environment.
        echo Expected at: !PROPOSED_PYTHON!
    )
) else (
    echo WARNING: Could not find Miniforge/Anaconda installation.
)

if "!MY_PYTHON_EXE!"=="" (
    echo Using default system python...
) else (
    echo Setting MY_PYTHON_EXE to !MY_PYTHON_EXE!
)

REM --------------------------------------------------

REM Set environment for OCCT and Qt (Clean environment for C++ App)
set PATH=D:\GitHub_Ymqhyq\OCCT\build-vs2022-x64\win64\vc14\bin;%PATH%
set PATH=D:\Qt\6.10.1\msvc2022_64\bin;%PATH%
set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT;%PATH%

REM Add 3rdparty DLLs
set PATH=D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\freetype-2.13.3-x64\bin;%PATH%
set PATH=D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\tbb-2021.13.0-x64\bin;%PATH%
set PATH=D:\GitHub_Ymqhyq\OCCT\3rdparty-vc14-64\freeimage-3.18.0-x64\bin;%PATH%

echo Ensuring run_cq.py is present...
copy /Y D:\QtOCCTApp\run_cq.py D:\QtOCCTApp\build2\Debug\run_cq.py >nul

echo Launching application...
cd /d "D:\QtOCCTApp\build2\Debug"

REM Pass the python path variable to the application
QtOCCTApp.exe

if %ERRORLEVEL% NEQ 0 (
    echo Application failed with error code: %ERRORLEVEL%
    pause
)
endlocal
