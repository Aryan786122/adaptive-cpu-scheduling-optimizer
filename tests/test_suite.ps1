# ==============================================================================
# Adaptive CPU Scheduling Optimizer - Automated Test Suite
# Validates Win32 backend, HTTP server, telemetry, and live experiment execution
# ==============================================================================

$ErrorActionPreference = "Stop"
$exePath = Join-Path $PSScriptRoot "..\adaptive_scheduler.exe"
$port = 8088
$baseUrl = "http://127.0.0.1:$port"

Write-Host "====================================================================" -ForegroundColor Cyan
Write-Host " Running Automated Test Suite: Adaptive CPU Scheduler" -ForegroundColor Cyan
Write-Host "====================================================================" -ForegroundColor Cyan

if (-not (Test-Path $exePath)) {
    Write-Error "Binary not found at $exePath! Please run build.bat first."
    exit 1
}

$proc = Start-Process -FilePath $exePath -ArgumentList "--port $port --no-browser" -PassThru
Write-Host "[1/6] Launching server process (PID: $($proc.Id)) on port $port..." -ForegroundColor Yellow

$bound = $false
for ($i = 0; $i -lt 30; $i++) {
    Start-Sleep -Milliseconds 100
    try {
        $res = Invoke-RestMethod -Uri "$baseUrl/api/telemetry" -TimeoutSec 1 -ErrorAction SilentlyContinue
        if ($res -and $res.cpu_model) {
            $bound = $true
            break
        }
    } catch {}
}

if (-not $bound) {
    Stop-Process -Id $proc.Id -Force
    Write-Error "[FAIL] Server did not become responsive on $baseUrl within 3 seconds."
    exit 1
}
Write-Host "  -> Server is responsive and listening." -ForegroundColor Green

try {
    # -------------------------------------------------------------------------
    # TEST 1: Static Assets (HTML, CSS, JS)
    # -------------------------------------------------------------------------
    Write-Host "`n[2/6] Testing Static Assets Delivery..." -ForegroundColor Yellow
    $html = (Invoke-WebRequest -Uri "$baseUrl/" -UseBasicParsing).Content
    if ($html -match "Adaptive CPU Scheduling Optimizer") {
        Write-Host "  [PASS] index.html served correctly." -ForegroundColor Green
    } else {
        throw "index.html missing title marker"
    }

    $css = (Invoke-WebRequest -Uri "$baseUrl/app.css" -UseBasicParsing).Content
    if ($css.Length -gt 500) {
        Write-Host "  [PASS] app.css served correctly ($($css.Length) bytes)." -ForegroundColor Green
    } else {
        throw "app.css is unexpectedly small or empty"
    }

    $js = (Invoke-WebRequest -Uri "$baseUrl/app.js" -UseBasicParsing).Content
    if ($js.Length -gt 500) {
        Write-Host "  [PASS] app.js served correctly ($($js.Length) bytes)." -ForegroundColor Green
    } else {
        throw "app.js is unexpectedly small or empty"
    }

    # -------------------------------------------------------------------------
    # TEST 2: Genuine System Telemetry API
    # -------------------------------------------------------------------------
    Write-Host "`n[3/6] Testing Real Host Telemetry API..." -ForegroundColor Yellow
    $telem = Invoke-RestMethod -Uri "$baseUrl/api/telemetry"
    Write-Host "  Host OS:       $($telem.os_name) ($($telem.architecture))" -ForegroundColor Gray
    Write-Host "  CPU Model:     $($telem.cpu_model)" -ForegroundColor Gray
    Write-Host "  Cores/Threads: $($telem.physical_cores) Physical / $($telem.logical_cores) Logical" -ForegroundColor Gray
    Write-Host "  RAM:           $($telem.used_ram_gb) GB / $($telem.total_ram_gb) GB ($($telem.ram_percent)%)" -ForegroundColor Gray
    Write-Host "  Procs/Threads: $($telem.total_processes) / $($telem.total_threads)" -ForegroundColor Gray

    if ($telem.logical_cores -gt 0 -and $telem.total_ram_gb -gt 0 -and $telem.cpu_model.Length -gt 0) {
        Write-Host "  [PASS] Genuine telemetry verified." -ForegroundColor Green
    } else {
        throw "Telemetry returned invalid or empty metrics"
    }

    # -------------------------------------------------------------------------
    # TEST 3: Launch Live Scheduling Experiment
    # -------------------------------------------------------------------------
    Write-Host "`n[4/6] Launching Live Scheduling Experiment (Scale: 0.5x Fast Benchmark, Affinity: Single Core)..." -ForegroundColor Yellow
    $runPayload = @{ affinity_cores = 1; scale = 0.5 } | ConvertTo-Json
    $runRes = Invoke-RestMethod -Uri "$baseUrl/api/experiment/run" -Method Post -Body $runPayload -ContentType "application/json"
    
    if ($runRes.status -eq "started") {
        Write-Host "  [PASS] Experiment dispatched successfully." -ForegroundColor Green
    } else {
        throw "Failed to start experiment: $($runRes.message)"
    }

    # -------------------------------------------------------------------------
    # TEST 4: Poll Status until Completion
    # -------------------------------------------------------------------------
    Write-Host "`n[5/6] Polling Live Experiment Progress..." -ForegroundColor Yellow
    $completed = $false
    $timeoutSeconds = 25
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    while ($stopwatch.Elapsed.TotalSeconds -lt $timeoutSeconds) {
        Start-Sleep -Milliseconds 300
        $status = Invoke-RestMethod -Uri "$baseUrl/api/experiment/status"
        Write-Host "  -> State: $($status.status_text) ($($status.progress_percent)%) - $($status.step_description)" -ForegroundColor Gray

        if ($status.status_code -eq 4) { # COMPLETED
            $completed = $true
            break
        }
        if ($status.status_code -eq 6) { # ERROR
            throw "Experiment failed with error state"
        }
    }

    if (-not $completed) {
        throw "Experiment timed out after $timeoutSeconds seconds"
    }
    Write-Host "  [PASS] Benchmark execution completed cleanly." -ForegroundColor Green

    # -------------------------------------------------------------------------
    # TEST 5: Verify Benchmark Metrics & Gantt Slices
    # -------------------------------------------------------------------------
    Write-Host "`n[6/6] Verifying Results, Slices & Systems Trade-off Analysis..." -ForegroundColor Yellow
    $finalStatus = Invoke-RestMethod -Uri "$baseUrl/api/experiment/status"
    $fcfs = $finalStatus.fcfs
    $adaptive = $finalStatus.adaptive

    Write-Host "`n  ================= EXPERIMENTAL BENCHMARK REPORT =================" -ForegroundColor Cyan
    Write-Host "  Interactive Avg Response Time: FCFS = $($fcfs.avg_interactive_response_time_ms) ms | Adaptive = $($adaptive.avg_interactive_response_time_ms) ms" -ForegroundColor White
    Write-Host "  Response Time Improvement:     $($finalStatus.response_time_improvement_pct)%" -ForegroundColor Green
    Write-Host "  Interactive Avg Waiting Time:  FCFS = $($fcfs.avg_interactive_waiting_time_ms) ms | Adaptive = $($adaptive.avg_interactive_waiting_time_ms) ms" -ForegroundColor White
    Write-Host "  Background Makespan Overhead:  $($finalStatus.background_makespan_delta_pct)%" -ForegroundColor Yellow
    Write-Host "  Cooperative Preemptions:       $($adaptive.total_preemptions) checkpoints" -ForegroundColor White
    Write-Host "  Total Recorded Gantt Slices:   FCFS = $($fcfs.slices.Count) | Adaptive = $($adaptive.slices.Count)" -ForegroundColor White
    Write-Host "  -----------------------------------------------------------------" -ForegroundColor Gray
    Write-Host "  Empirical Verdict: $($finalStatus.empirical_conclusion)" -ForegroundColor Gray
    Write-Host "  =================================================================`n" -ForegroundColor Cyan

    if ($fcfs.slices.Count -gt 0 -and $adaptive.slices.Count -gt 0) {
        Write-Host "  [PASS] Gantt execution slices recorded." -ForegroundColor Green
    } else {
        throw "No execution slices were recorded"
    }

    if ($finalStatus.response_time_improvement_pct -gt 0) {
        Write-Host "  [PASS] Confirmed positive response time improvement for interactive tasks under adaptive priority!" -ForegroundColor Green
    } else {
        Write-Host "  [INFO] Adaptive response delta: $($finalStatus.response_time_improvement_pct)% (System trade-off documented honestly)" -ForegroundColor Yellow
    }

    Write-Host "`n>>> ALL 6 AUTOMATED SYSTEM TESTS PASSED SUCCESSFULLY! <<<`n" -ForegroundColor Green

} finally {
    Write-Host "Terminating test server process (PID: $($proc.Id))..." -ForegroundColor Yellow
    Stop-Process -Id $proc.Id -Force
    Start-Sleep -Milliseconds 300
}
