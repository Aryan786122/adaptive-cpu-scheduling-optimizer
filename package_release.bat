@echo off
setlocal enabledelayedexpansion

echo ====================================================================
echo Packaging Standalone Portable Release Artifact
echo ====================================================================

if not exist "adaptive_scheduler.exe" (
    echo [Info] Binary not found. Building first...
    call build.bat
    if %errorlevel% neq 0 (
        echo [Error] Build failed! Cannot package release.
        exit /b 1
    )
)

set DIST_DIR=dist
set STAGING_DIR=%DIST_DIR%\adaptive-cpu-scheduler
set ZIP_FILE=%DIST_DIR%\adaptive-cpu-scheduler-windows-x64.zip

if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%STAGING_DIR%"
mkdir "%STAGING_DIR%\frontend"

echo [1/3] Staging executable and web assets...
copy /y "adaptive_scheduler.exe" "%STAGING_DIR%\" >nul
xcopy /s /e /y "frontend" "%STAGING_DIR%\frontend\" >nul
if exist "README.md" copy /y "README.md" "%STAGING_DIR%\" >nul

echo [2/3] Compressing into standalone ZIP archive...
powershell -Command "Compress-Archive -Path '%STAGING_DIR%\*' -DestinationPath '%ZIP_FILE%' -Force"

if not exist "%ZIP_FILE%" (
    echo [Error] Failed to create ZIP archive!
    exit /b 1
)

echo.
echo [3/3] Packaging successful!
echo Archive created: %~dp0%ZIP_FILE%
echo.
echo The archive contains:
echo   - adaptive_scheduler.exe (Native Win32 C99 binary)
echo   - frontend\ (HTML, CSS, JS single-screen interface)
echo.
echo Zero Python or Node.js runtime required on target machine!
echo ====================================================================
exit /b 0
