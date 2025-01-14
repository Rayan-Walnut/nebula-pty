@echo off
echo Checking dependencies...
echo =====================

set /a missing=0

gcc --version >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [OK] MinGW found
) else (
    echo [X] MinGW not found - Get it from: https://sourceforge.net/projects/mingw/
    set /a missing+=1
)

node --version >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [OK] Node.js found
) else (
    echo [X] Node.js not found - Get it from: https://nodejs.org/
    set /a missing+=1
)

python --version >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [OK] Python found
) else (
    echo [X] Python not found - Get it from: https://www.python.org/downloads/
    set /a missing+=1
)

echo =====================

if %missing% GTR 0 (
    echo Missing: %missing% dependencies
) else (
    echo All dependencies found!
    echo.
    echo Installed versions:
    gcc --version | findstr /i "gcc"
    cmake --version | findstr /i "version"
    node --version
    python --version
)

exit /b %missing%