@echo off
setlocal enabledelayedexpansion

echo ====================================================================
echo Building Adaptive CPU Scheduling Optimizer (Win32 Native C99)
echo ====================================================================

where gcc >nul 2>nul
if %errorlevel% neq 0 (
    echo [Error] GCC compiler not found in PATH!
    echo Please install MinGW-w64 or MSYS2 GCC and add it to PATH.
    exit /b 1
)

echo [1/2] Compiling C source files with GCC (-O2 -Wall -std=c99)...
gcc -O2 -Wall -Wextra -std=c99 -D_WIN32_WINNT=0x0601 ^
    backend\main.c ^
    backend\system_telemetry.c ^
    backend\calibration.c ^
    backend\workload_generator.c ^
    backend\scheduler_fcfs.c ^
    backend\scheduler_adaptive.c ^
    backend\experiment_runner.c ^
    backend\http_server.c ^
    backend\json_builder.c ^
    -o adaptive_scheduler.exe ^
    -lws2_32 -lpsapi -liphlpapi

if %errorlevel% neq 0 (
    echo.
    echo [Fatal] Compilation failed!
    exit /b %errorlevel%
)

echo.
echo [2/2] Build successful!
echo Binary created: %~dp0adaptive_scheduler.exe
echo.
echo To run the application:
echo   adaptive_scheduler.exe
echo.
exit /b 0
