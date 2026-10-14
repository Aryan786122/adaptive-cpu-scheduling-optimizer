# ====================================================================
# Standalone Portable Packaging Smoke Test
# ====================================================================
$ErrorActionPreference = "Stop"

$zipPath = ".\dist\adaptive-cpu-scheduler-windows-x64.zip"
if (-not (Test-Path $zipPath)) {
    Write-Error "Release archive $zipPath does not exist!"
    exit 1
}

$testDir = Join-Path $env:TEMP ("sched_smoke_" + (Get-Random))
Write-Host "[1/5] Extracting ZIP to temporary testbed: $testDir"
Expand-Archive -Path $zipPath -DestinationPath $testDir -Force

try {
    # Check binary exists
    $exePath = Join-Path $testDir "adaptive_scheduler.exe"
    if (-not (Test-Path $exePath)) {
        Write-Error "adaptive_scheduler.exe missing from extracted archive!"
        exit 1
    }
    Write-Host "  [PASS] adaptive_scheduler.exe present ($((Get-Item $exePath).Length) bytes)."

    # Check frontend assets exist
    $htmlPath = Join-Path $testDir "frontend\index.html"
    $cssPath = Join-Path $testDir "frontend\app.css"
    $jsPath = Join-Path $testDir "frontend\app.js"
    if (-not (Test-Path $htmlPath) -or -not (Test-Path $cssPath) -or -not (Test-Path $jsPath)) {
        Write-Error "Frontend web assets missing from extracted archive!"
        exit 1
    }
    Write-Host "  [PASS] Frontend static assets present."

    # Launch standalone process on port 8098
    $smokePort = 8098
    Write-Host "[2/5] Launching standalone executable on port $smokePort..."
    $proc = Start-Process -FilePath $exePath -ArgumentList "--port $smokePort --no-browser" -PassThru
    Start-Sleep -Seconds 2

    # Query telemetry
    Write-Host "[3/5] Querying Win32 telemetry API on standalone instance..."
    $response = Invoke-RestMethod -Uri "http://127.0.0.1:$smokePort/api/telemetry" -TimeoutSec 5
    if (-not $response -or -not $response.cpu_model) {
        Write-Error "Failed to obtain telemetry from standalone process!"
        exit 1
    }
    Write-Host "  [PASS] Standalone instance returned genuine telemetry: $($response.cpu_model)"

    # Query index.html
    Write-Host "[4/5] Verifying static asset delivery from standalone instance..."
    $htmlContent = Invoke-WebRequest -Uri "http://127.0.0.1:$smokePort/" -TimeoutSec 5
    if ($htmlContent.StatusCode -ne 200 -or -not ($htmlContent.Content -like "*Adaptive CPU Scheduling*")) {
        Write-Error "HTML content delivery verification failed!"
        exit 1
    }
    Write-Host "  [PASS] Standalone instance successfully served full laboratory interface."

    Write-Host "[5/5] Cleaning up test process..."
    Stop-Process -Id $proc.Id -Force
    Start-Sleep -Milliseconds 500

    Write-Host ">>> PACKAGING SMOKE TEST PASSED SUCCESSFULLY! <<<"
}
finally {
    if (Test-Path $testDir) {
        Remove-Item -Path $testDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}
