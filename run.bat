@echo off
REM ============================================================
REM  QEdgeOpt - One-Click Build and Run (Windows)
REM  Quantum-Inspired Edge Microservice Placement Optimizer
REM ============================================================

echo.
echo  ==================================================
echo     QEdgeOpt - Build ^& Launch                      
echo     Quantum-Inspired Edge Placement Optimizer       
echo  ==================================================
echo.

REM -- Add Winget Paths to PATH if they exist ------------------
set "CMAKE_PATH=C:\Program Files\CMake\bin"
if exist "%CMAKE_PATH%" (
    set "PATH=%CMAKE_PATH%;%PATH%"
)

set "MINGW_PATH=C:\Users\Chakku\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
if exist "%MINGW_PATH%" (
    set "PATH=%MINGW_PATH%;%PATH%"
)

REM -- Check CMake -----------------------------------------
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake not found. Please install CMake and add to PATH.
    echo         https://cmake.org/download/
    pause
    exit /b 1
)

REM -- Check for a C++ compiler ----------------------------
where cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    where g++ >nul 2>&1
    if %ERRORLEVEL% neq 0 (
        echo [WARNING] No C++ compiler found in PATH.
        echo           Ensure Visual Studio or MinGW is installed.
    )
)

REM -- Configure -------------------------------------------
echo [1/3] Configuring with CMake...
if not exist build mkdir build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b 1
)

REM -- Build -----------------------------------------------
echo.
echo [2/3] Building QEdgeOpt...
cmake --build build --config Release --parallel
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

REM -- Find the executable ---------------------------------
set "EXE="
if exist build\Release\qedgeopt.exe (
    set "EXE=build\Release\qedgeopt.exe"
) else if exist build\qedgeopt.exe (
    set "EXE=build\qedgeopt.exe"
) else (
    echo [ERROR] Cannot find qedgeopt.exe
    pause
    exit /b 1
)

REM -- Launch ----------------------------------------------
echo.
echo [3/3] Launching QEdgeOpt server...
echo        Dashboard: http://localhost:8080
echo        Press Ctrl+C to stop.
echo.

REM Open browser after a short delay
start "" "http://localhost:8080"

%EXE%

pause
