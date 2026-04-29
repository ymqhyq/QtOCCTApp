@echo off
setlocal
:: Pass the first argument (debug/release) to env.bat
call "%~dp0env.bat" %1

echo.
echo ==================================================
echo   ActiveData Runtime Environment Test
echo ==================================================

set "MISSING_COUNT=0"
set "DEPS=asiActiveData.dll TKernel.dll vtkCommonCore-9.4.dll tbb12.dll freetype.dll FreeImage.dll"

for %%i in (%DEPS%) do (
    where %%i >nul 2>&1
    if errorlevel 1 (
        echo [FAIL] %%i is MISSING from PATH
        set /a MISSING_COUNT+=1
    ) else (
        for /f "tokens=*" %%j in ('where %%i') do echo [OK] Found %%i at: %%j
    )
)

echo --------------------------------------------------
if %MISSING_COUNT% equ 0 (
    echo [SUCCESS] All core libraries are correctly configured.
) else (
    echo [ERROR] %MISSING_COUNT% dependencies are missing.
)
echo ==================================================
