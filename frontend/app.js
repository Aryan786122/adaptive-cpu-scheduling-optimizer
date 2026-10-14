/**
 * Adaptive CPU Scheduling Optimizer - Guided Laboratory Controller
 * University OS Demonstration Engine (V3)
 */

// Global State
let telemPollTimer = null;
let capturePollTimer = null;
let expPollTimer = null;
let isExperimentRunning = false;
let currentRunMode = 'idle'; // 'fcfs', 'adaptive', 'idle'
let selectedMode = 'standard';
let selectedIntensity = 'balanced';
let lastExperimentData = null;
let replayTimer = null;
let replayPlaying = false;
let replaySpeed = 1.0;
let replayProgress = 100; // 0 - 100%

// -----------------------------------------------------------------------------
// STAGE 1: REAL-TIME TELEMETRY & SYSTEM CAPTURE
// -----------------------------------------------------------------------------
async function pollTelemetry() {
  try {
    const res = await fetch('/api/telemetry');
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const data = await res.json();

    // Connection Pill
    const connStatus = document.getElementById('conn-status-text');
    if (connStatus) connStatus.textContent = `Backend Connected (127.0.0.1:${window.location.port || '8000'})`;

    // Hardware Specs
    const elCpuModel = document.getElementById('cpu-model-val');
    if (elCpuModel) elCpuModel.textContent = data.cpu_model || 'Host Windows Processor';

    const elCpuCores = document.getElementById('cpu-cores-val');
    if (elCpuCores) elCpuCores.textContent = `${data.physical_cores || 0} Physical Cores / ${data.logical_cores || 0} Logical Processors`;

    // CPU Usage & Sparkline
    const cpuPct = data.cpu_percent !== undefined ? data.cpu_percent.toFixed(1) : '0.0';
    const elCpuUsage = document.getElementById('cpu-usage-val');
    if (elCpuUsage) elCpuUsage.textContent = `${cpuPct}%`;

    const elCpuDesc = document.getElementById('cpu-usage-desc');
    if (elCpuDesc) elCpuDesc.textContent = `Real-time Host Load (Active: ${data.total_threads || 0} threads)`;

    if (data.history && data.history.length > 1) {
      renderSparkline(data.history.map(h => h.p));
    }

    // RAM Usage
    const elRamVal = document.getElementById('ram-usage-val');
    if (elRamVal && data.total_ram_gb) {
      elRamVal.textContent = `${data.used_ram_gb.toFixed(1)} / ${data.total_ram_gb.toFixed(1)} GB`;
    }
    const elRamPct = document.getElementById('ram-percent-val');
    if (elRamPct && data.ram_percent !== undefined) {
      elRamPct.textContent = `${data.ram_percent.toFixed(0)}% Physical Memory Load`;
    }

    // Processes & Threads
    const elProcs = document.getElementById('procs-count-val');
    if (elProcs) elProcs.textContent = `${data.total_processes || 0} Processes Active`;

    const elThreads = document.getElementById('threads-count-val');
    if (elThreads) elThreads.textContent = `${data.total_threads || 0} Threads Running`;

  } catch (err) {
    const connStatus = document.getElementById('conn-status-text');
    if (connStatus) connStatus.textContent = 'Backend Reconnecting...';
  }
}

function renderSparkline(points) {
  const path = document.getElementById('sparkline-path');
  if (!path) return;
  const width = 100;
  const height = 24;
  const coords = points.map((val, idx) => {
    const x = (idx / (points.length - 1)) * width;
    const y = height - (Math.min(100, Math.max(0, val)) / 100) * height;
    return `${x.toFixed(1)},${y.toFixed(1)}`;
  });
  path.setAttribute('d', `M ${coords.join(' L ')}`);
}

async function startSystemCapture() {
  const btnCapture = document.getElementById('btn-capture');
  const btnLabel = document.getElementById('btn-capture-label');
  const progWrapper = document.getElementById('capture-progress-wrapper');

  btnCapture.disabled = true;
  btnLabel.textContent = 'Capturing Host Activity...';
  progWrapper.style.display = 'block';

  try {
    const res = await fetch('/api/telemetry/capture', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ duration_seconds: 5 })
    });
    if (!res.ok) throw new Error('Capture request failed');

    if (capturePollTimer) clearInterval(capturePollTimer);
    capturePollTimer = setInterval(pollCaptureStatus, 200);
  } catch (err) {
    alert('Failed to start system activity capture: ' + err.message);
    btnCapture.disabled = false;
    btnLabel.textContent = 'Capture System Activity (5s Sample)';
    progWrapper.style.display = 'none';
  }
}

async function pollCaptureStatus() {
  try {
    const res = await fetch('/api/telemetry/capture_status');
    if (!res.ok) return;
    const data = await res.json();

    const progBar = document.getElementById('capture-progress-bar');
    const progLabel = document.getElementById('capture-progress-label');

    progBar.style.width = `${data.progress_percent}%`;
    progLabel.textContent = `Sampling active Win32 process CPU deltas... (${data.progress_percent.toFixed(0)}%)`;

    if (data.is_complete) {
      clearInterval(capturePollTimer);
      capturePollTimer = null;

      const btnCapture = document.getElementById('btn-capture');
      const btnLabel = document.getElementById('btn-capture-label');
      const progWrapper = document.getElementById('capture-progress-wrapper');

      btnCapture.disabled = false;
      btnLabel.textContent = 'Re-Capture System Activity';
      progWrapper.style.display = 'none';

      // Render top processes
      const topContainer = document.getElementById('top-processes-container');
      if (data.top_processes && data.top_processes.length > 0) {
        let html = '';
        data.top_processes.forEach(p => {
          html += `
            <div class="proc-chip">
              <div class="proc-info">
                <span class="proc-name">${p.name}</span>
                <span class="proc-pid">PID ${p.pid}</span>
              </div>
              <span class="proc-cpu">${p.cpu_percent.toFixed(1)}% CPU</span>
            </div>
          `;
        });
        topContainer.innerHTML = html;
      }

      // Summary text
      const summaryText = document.getElementById('capture-summary-text');
      if (summaryText && data.summary) {
        summaryText.innerHTML = `<strong>Telemetry Provenance:</strong> ${data.summary}`;
      }

      // Automatically select Captured mode in Stage 2
      selectWorkloadMode('captured');
    }
  } catch (err) {
    console.error('Capture status poll error:', err);
  }
}

// -----------------------------------------------------------------------------
// STAGE 2: WORKLOAD CONFIGURATION & TASK CARDS
// -----------------------------------------------------------------------------
function selectWorkloadMode(mode) {
  selectedMode = mode;
  document.getElementById('btn-mode-standard').classList.toggle('active', mode === 'standard');
  document.getElementById('btn-mode-captured').classList.toggle('active', mode === 'captured');
  prepareActiveWorkload();
}

function selectWorkloadIntensity(intensity) {
  selectedIntensity = intensity;
  document.getElementById('btn-int-light').classList.toggle('active', intensity === 'light');
  document.getElementById('btn-int-balanced').classList.toggle('active', intensity === 'balanced');
  document.getElementById('btn-int-heavy').classList.toggle('active', intensity === 'heavy');
  prepareActiveWorkload();
}

async function prepareActiveWorkload() {
  const container = document.getElementById('prepared-tasks-grid');
  try {
    const res = await fetch('/api/workload/prepare', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ mode: selectedMode, intensity: selectedIntensity })
    });
    if (!res.ok) throw new Error('Workload preparation failed');
    const data = await res.json();

    // Render 3 Task Cards
    if (data.tasks && data.tasks.length > 0) {
      let cardsHtml = '';
      data.tasks.forEach(t => {
        let typeClass = 'type-compute';
        let badgeClass = 'tcp-badge-compute';
        let typeName = 'Compute Worker';
        if (t.type === 0) {
          typeClass = 'type-interactive';
          badgeClass = 'tcp-badge-interactive';
          typeName = 'Interactive Request';
        } else if (t.type === 2) {
          typeClass = 'type-memory';
          badgeClass = 'tcp-badge-memory';
          typeName = 'Memory Worker';
        }

        cardsHtml += `
          <div class="task-card-prep ${typeClass}">
            <div class="tcp-header">
              <span class="tcp-title">${t.name}</span>
              <span class="tcp-badge ${badgeClass}">${typeName}</span>
            </div>
            <p class="tcp-desc">${t.desc}</p>
            <div class="tcp-stats-box">
              <div class="tcp-stat-row">
                <span class="tcp-stat-label">Base Priority:</span>
                <span class="tcp-stat-value">Priority ${t.priority} ${t.priority === 1 ? '(High)' : '(Background)'}</span>
              </div>
              <div class="tcp-stat-row">
                <span class="tcp-stat-label">Calibrated Duration:</span>
                <span class="tcp-stat-value">${t.target_ms.toFixed(0)} ms</span>
              </div>
              <div class="tcp-stat-row">
                <span class="tcp-stat-label">Scheduled Arrival:</span>
                <span class="tcp-stat-value">t = ${t.arrival_offset.toFixed(0)} ms</span>
              </div>
            </div>
          </div>
        `;
      });
      container.innerHTML = cardsHtml;
    }

    // Reset Stage 3 buttons
    document.getElementById('btn-run-fcfs').disabled = false;
    document.getElementById('fcfs-btn-status').textContent = 'Ready to run FCFS';
    document.getElementById('btn-run-adaptive').disabled = true;
    document.getElementById('adaptive-btn-status').textContent = 'Locked &bull; Run FCFS first';
    document.getElementById('btn-urgent').disabled = true;
    document.getElementById('btn-urgent').classList.remove('pulsing');
    document.getElementById('urgent-btn-status').textContent = 'Active only during FCFS execution';

    const phenBanner = document.getElementById('phenomenon-banner');
    if (phenBanner) phenBanner.style.display = 'none';

  } catch (err) {
    container.innerHTML = `<div class="task-card-placeholder">Error preparing workload: ${err.message}</div>`;
  }
}

// -----------------------------------------------------------------------------
// STAGE 3: DUAL EXECUTION ACTIONS & LIVE DISPATCH BOARD
// -----------------------------------------------------------------------------
async function runFcfsExperiment() {
  if (isExperimentRunning) return;

  isExperimentRunning = true;
  currentRunMode = 'fcfs';

  // Button States
  document.getElementById('btn-run-fcfs').disabled = true;
  document.getElementById('fcfs-btn-status').textContent = 'Running FCFS on Core 0...';
  document.getElementById('btn-run-adaptive').disabled = true;
  document.getElementById('btn-cancel-exp').disabled = false;

  // Enable Urgent Injection button with pulse!
  const btnUrgent = document.getElementById('btn-urgent');
  btnUrgent.disabled = false;
  btnUrgent.classList.add('pulsing');
  document.getElementById('urgent-btn-status').textContent = '⚡ Click now to inject an urgent interactive task!';

  // Reset phenomenon banner
  const phenBanner = document.getElementById('phenomenon-banner');
  phenBanner.className = 'phenomenon-alert-banner';
  phenBanner.style.display = 'none';

  document.getElementById('lab-status-msg').textContent = 'Executing non-preemptive FCFS on Core 0. Observe B1 blocking arriving requests...';

  try {
    const res = await fetch('/api/experiment/run_fcfs', { method: 'POST' });
    if (!res.ok) throw new Error('FCFS launch failed');

    if (expPollTimer) clearInterval(expPollTimer);
    expPollTimer = setInterval(pollExperimentStatus, 100);
  } catch (err) {
    alert('Failed to launch FCFS: ' + err.message);
    resetExperimentButtons();
  }
}

async function injectUrgentRequest() {
  try {
    const res = await fetch('/api/experiment/inject_task', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: 'I_Urgent_User_Click' })
    });
    if (!res.ok) {
      const err = await res.json();
      throw new Error(err.message || 'Injection failed');
    }

    const urgentStatus = document.getElementById('urgent-btn-status');
    urgentStatus.textContent = '✔ Urgent Request Injected! Blocked in Ready Queue behind B1.';
    urgentStatus.style.color = '#38bdf8';
  } catch (err) {
    console.error('Urgent injection error:', err);
  }
}

async function runAdaptiveExperiment() {
  if (isExperimentRunning) return;

  isExperimentRunning = true;
  currentRunMode = 'adaptive';

  // Button States
  document.getElementById('btn-run-adaptive').disabled = true;
  document.getElementById('adaptive-btn-status').textContent = 'Running Adaptive on identical workload...';
  document.getElementById('btn-run-fcfs').disabled = true;
  document.getElementById('btn-urgent').disabled = true;
  document.getElementById('btn-urgent').classList.remove('pulsing');
  document.getElementById('btn-cancel-exp').disabled = false;

  // Show Preemption alert banner
  const phenBanner = document.getElementById('phenomenon-banner');
  phenBanner.className = 'phenomenon-alert-banner banner-adaptive';
  document.getElementById('phenomenon-icon').textContent = '⚡';
  document.getElementById('phenomenon-title').textContent = 'Adaptive Preemption & Aging Active';
  document.getElementById('phenomenon-text').innerHTML = 'Core 0 execution uses <strong>~15ms cooperative checkpoints</strong>. Higher-priority interactive requests preempt background computation immediately!';
  phenBanner.style.display = 'flex';

  document.getElementById('lab-status-msg').textContent = 'Executing Adaptive Priority Scheduler. Watching preemption checkpoint yield...';

  try {
    const res = await fetch('/api/experiment/run_adaptive', { method: 'POST' });
    if (!res.ok) throw new Error('Adaptive launch failed');

    if (expPollTimer) clearInterval(expPollTimer);
    expPollTimer = setInterval(pollExperimentStatus, 100);
  } catch (err) {
    alert('Failed to launch Adaptive run: ' + err.message);
    resetExperimentButtons();
  }
}

async function cancelExperiment() {
  document.getElementById('lab-status-msg').textContent = 'Cancelling experiment run...';
  try {
    await fetch('/api/experiment/cancel', { method: 'POST' });
  } catch (e) {}
}

function resetExperimentButtons() {
  isExperimentRunning = false;
  currentRunMode = 'idle';
  document.getElementById('btn-cancel-exp').disabled = true;
  document.getElementById('btn-urgent').disabled = true;
  document.getElementById('btn-urgent').classList.remove('pulsing');
  document.getElementById('urgent-btn-status').textContent = 'Active only during FCFS execution';

  if (expPollTimer) {
    clearInterval(expPollTimer);
    expPollTimer = null;
  }
}

async function pollExperimentStatus() {
  try {
    const res = await fetch('/api/experiment/status');
    if (!res.ok) return;
    const data = await res.json();
    lastExperimentData = data;

    document.getElementById('lab-status-msg').textContent = data.step_description || data.status_text;

    // Status:
    // 0: IDLE, 1: CALIBRATING, 2: RUNNING_FCFS, 3: FCFS_DONE, 4: RUNNING_ADAPTIVE, 5: COMPLETED, 6: CANCELLED
    const code = data.status_code;

    // Live Dispatch Lanes Animation
    updateDispatchBoard(data);

    // Convoy Effect alert in FCFS
    if (code === 2) {
      // FCFS Running
      const phenBanner = document.getElementById('phenomenon-banner');
      if (data.elapsed_seconds > 0.4) {
        phenBanner.className = 'phenomenon-alert-banner';
        document.getElementById('phenomenon-icon').textContent = '⚠️';
        document.getElementById('phenomenon-title').textContent = 'Convoy Effect In Progress';
        const liveWaitMs = ((data.elapsed_seconds - 0.4) * 1000).toFixed(0);
        document.getElementById('phenomenon-text').innerHTML = `Interactive task I1 is trapped behind B1! Accumulated queue delay: <span id="convoy-live-timer">${liveWaitMs} ms</span>`;
        phenBanner.style.display = 'flex';
      }
    } else if (code === 3) {
      // FCFS_DONE!
      resetExperimentButtons();
      document.getElementById('btn-run-fcfs').disabled = false;
      document.getElementById('fcfs-btn-status').textContent = `✔ FCFS Done (Interactive delay: ${data.fcfs.avg_interactive_response_time_ms.toFixed(0)}ms)`;
      
      // Unlock Step 3B (Adaptive)!
      const btnAdapt = document.getElementById('btn-run-adaptive');
      btnAdapt.disabled = false;
      btnAdapt.classList.add('btn-run-adaptive');
      document.getElementById('adaptive-btn-status').textContent = 'Ready! Click to run Adaptive on the SAME workload.';

      // Render partial Gantt for FCFS
      if (data.fcfs && data.fcfs.slices) {
        renderGanttSlices(document.getElementById('fcfs-gantt-track'), data.fcfs.slices, data.fcfs.total_benchmark_duration_ms);
        document.getElementById('fcfs-span-meta').textContent = `${data.fcfs.total_benchmark_duration_ms.toFixed(0)} ms`;
      }
    } else if (code === 4) {
      // Running Adaptive
      // Checkpoint alert
      const phenBanner = document.getElementById('phenomenon-banner');
      phenBanner.className = 'phenomenon-alert-banner banner-adaptive';
      document.getElementById('phenomenon-icon').textContent = '⚡';
      document.getElementById('phenomenon-title').textContent = 'Cooperative Checkpoint Yielded';
      phenBanner.style.display = 'flex';
    } else if (code >= 5) {
      // COMPLETED or CANCELLED
      resetExperimentButtons();
      if (code === 5) {
        document.getElementById('btn-run-fcfs').disabled = false;
        document.getElementById('btn-run-adaptive').disabled = false;
        document.getElementById('adaptive-btn-status').textContent = '✔ Adaptive Complete';
        displayResults(data);
      }
    }
  } catch (err) {
    console.error('Experiment poll error:', err);
  }
}

function updateDispatchBoard(data) {
  if (!data.tasks || data.tasks.length === 0) return;

  const isRunning = (data.status_code === 2 || data.status_code === 4);
  const isFcfs = (data.status_code === 2);

  const elReadyBadge = document.getElementById('ready-count-badge');
  const elReadyItems = document.getElementById('ready-items-container');
  const elCoreSlot = document.getElementById('core-slot-display');
  const elCoreState = document.getElementById('core-state-badge');
  const elCompletedBadge = document.getElementById('completed-count-badge');
  const elCompletedItems = document.getElementById('completed-items-container');

  if (isRunning) {
    elCoreState.textContent = isFcfs ? 'Executing (FCFS)' : 'Executing (Adaptive)';
    elCoreState.className = 'lane-badge badge-active';

    // Populate Ready Queue
    let readyHtml = '';
    data.tasks.forEach(t => {
      const typeLabel = t.type === 0 ? 'Interactive' : (t.type === 1 ? 'Compute' : 'Memory');
      readyHtml += `
        <div class="lane-card type-${t.type}">
          <div>
            <div class="lane-card-title">${t.name}</div>
            <div class="lane-card-meta">${typeLabel} &bull; Target: ${t.target_ms.toFixed(0)}ms</div>
          </div>
          <span class="lane-card-badge">Pri ${t.priority}</span>
        </div>
      `;
    });
    elReadyItems.innerHTML = readyHtml;
    elReadyBadge.textContent = `${data.tasks.length} Tasks`;

    // Active Core display
    elCoreSlot.innerHTML = `
      <div class="core-occupied-card">
        <div class="core-occ-title">
          <span>Core 0 (Multiprogramming Locked)</span>
          <span style="color: #60a5fa;">${isFcfs ? 'Non-Preemptive FCFS' : 'Cooperative Preemption (~15ms)'}</span>
        </div>
        <div class="core-occ-progress">
          <div class="core-occ-fill"></div>
        </div>
        <div style="font-size: 11px; color: #94a3b8; display: flex; justify-content: space-between;">
          <span>Active Contention</span>
          <span>${data.elapsed_seconds.toFixed(1)}s elapsed</span>
        </div>
      </div>
    `;
  } else if (data.status_code === 3 || data.status_code === 5) {
    elCoreState.textContent = 'Idle';
    elCoreState.className = 'lane-badge';
    elReadyBadge.textContent = '0 Tasks';
    elReadyItems.innerHTML = '<div class="lane-empty-placeholder">Ready queue idle.</div>';

    elCoreSlot.innerHTML = `
      <div class="core-unoccupied" style="color: var(--color-success);">
        <span class="core-icon">✔</span>
        <span>Run Concluded &bull; All Tasks Executed to Completion</span>
      </div>
    `;

    let compHtml = '';
    data.tasks.forEach(t => {
      compHtml += `
        <div class="lane-card type-${t.type}">
          <span class="lane-card-title">${t.name}</span>
          <span style="color: var(--color-success); font-weight: 700; font-size: 10px;">Finished</span>
        </div>
      `;
    });
    elCompletedItems.innerHTML = compHtml;
    elCompletedBadge.textContent = `${data.tasks.length} Finished`;
  }
}

// -----------------------------------------------------------------------------
// STAGE 4: RESULTS, GANTT TIMELINE & TRADE-OFF EXPLANATION
// -----------------------------------------------------------------------------
function displayResults(data) {
  const fcfs = data.fcfs;
  const adapt = data.adaptive;
  if (!fcfs || !adapt) return;

  // 1. Mandatory Exact Format Verdict Banner
  const elVerdictPrimary = document.getElementById('verdict-primary-text');
  const elVerdictSecondary = document.getElementById('verdict-secondary-text');

  elVerdictPrimary.textContent = data.empirical_conclusion ||
    `Adaptive scheduling reduced interactive request latency by ${data.response_time_improvement_pct.toFixed(1)}%, while background completion time changed by ${data.background_makespan_delta_pct >= 0 ? '+' : ''}${data.background_makespan_delta_pct.toFixed(1)}%.`;

  elVerdictSecondary.textContent = data.trade_off_analysis ||
    `Under non-preemptive FCFS, short interactive tasks were trapped in the ready queue behind long background computation (The Convoy Effect). The Adaptive Priority Scheduler enabled interactive requests to gain immediate CPU attention at safe cooperative checkpoints (${data.preemption_count} preemptions recorded). Dynamic aging prevented background starvation, ensuring all computational work completed reliably.`;

  // 2. Metric Cards
  document.getElementById('val-fcfs-resp').textContent = `${fcfs.avg_interactive_response_time_ms.toFixed(1)} ms`;
  document.getElementById('val-adapt-resp').textContent = `${adapt.avg_interactive_response_time_ms.toFixed(1)} ms`;
  const respImp = data.response_time_improvement_pct;
  const badgeResp = document.getElementById('badge-resp-pct');
  badgeResp.textContent = respImp >= 0 ? `+${respImp.toFixed(1)}% Faster Latency` : `${respImp.toFixed(1)}%`;
  badgeResp.className = respImp >= 0 ? 'rm-pill-badge' : 'rm-pill-tradeoff';

  document.getElementById('val-fcfs-wait').textContent = `${fcfs.avg_interactive_waiting_time_ms.toFixed(1)} ms`;
  document.getElementById('val-adapt-wait').textContent = `${adapt.avg_interactive_waiting_time_ms.toFixed(1)} ms`;
  const waitImp = data.waiting_time_improvement_pct;
  const badgeWait = document.getElementById('badge-wait-pct');
  badgeWait.textContent = waitImp >= 0 ? `+${waitImp.toFixed(1)}% Less Wait` : `${waitImp.toFixed(1)}%`;
  badgeWait.className = waitImp >= 0 ? 'rm-pill-badge' : 'rm-pill-tradeoff';

  document.getElementById('val-fcfs-make').textContent = `${fcfs.background_makespan_ms.toFixed(1)} ms`;
  document.getElementById('val-adapt-make').textContent = `${adapt.background_makespan_ms.toFixed(1)} ms`;
  const makeDelta = data.background_makespan_delta_pct;
  document.getElementById('badge-make-pct').textContent = `${makeDelta >= 0 ? '+' : ''}${makeDelta.toFixed(1)}% Overhead`;

  // 3. Performance Comparison Chart (Native SVG)
  renderComparisonChart(fcfs, adapt);

  // 4. Gantt Execution Timelines
  const maxSpan = Math.max(fcfs.total_benchmark_duration_ms, adapt.total_benchmark_duration_ms, 1.0);
  document.getElementById('timeline-span-badge').textContent = `Total Execution Span: ${maxSpan.toFixed(0)} ms`;

  document.getElementById('fcfs-span-meta').textContent = `${fcfs.total_benchmark_duration_ms.toFixed(0)} ms`;
  document.getElementById('adaptive-span-meta').textContent = `${adapt.total_benchmark_duration_ms.toFixed(0)} ms &bull; ${data.preemption_count} Checkpoints`;

  document.getElementById('axis-label-25').textContent = `${(maxSpan * 0.25).toFixed(0)} ms`;
  document.getElementById('axis-label-50').textContent = `${(maxSpan * 0.50).toFixed(0)} ms`;
  document.getElementById('axis-label-75').textContent = `${(maxSpan * 0.75).toFixed(0)} ms`;
  document.getElementById('axis-label-100').textContent = `${maxSpan.toFixed(0)} ms`;

  renderGanttSlices(document.getElementById('fcfs-gantt-track'), fcfs.slices, maxSpan);
  renderGanttSlices(document.getElementById('adaptive-gantt-track'), adapt.slices, maxSpan);

  // 5. Technical Details Panel
  if (data.adaptive_ctx) {
    document.getElementById('telem-evidence-text').textContent =
      `Host CPU Load Observed:  ${data.adaptive_ctx.host_cpu.toFixed(1)}%\n` +
      `Dynamic Aging Threshold: ${data.adaptive_ctx.aging_thresh} ms\n` +
      `Cooperative Interval:    ~${data.adaptive_ctx.checkpoint_ms.toFixed(1)} ms\n` +
      `Telemetry Reasoning:     ${data.adaptive_ctx.reason}`;
  }

  document.getElementById('preemption-evidence-text').textContent =
    `Cooperative Preemptions: ${data.preemption_count} checkpoints yielded\n` +
    `Context Switch Count:    ${adapt.total_context_switches} (vs ${fcfs.total_context_switches} in FCFS, Delta: ${data.context_switch_delta})\n` +
    `Hardware Timer:          Win32 QueryPerformanceCounter (Sub-microsecond frequency)\n` +
    `CPU Affinity Mask:       0x1 (Core 0 locked for multiprogramming contention)`;

  // Populate Transitions Table
  if (adapt.transitions && adapt.transitions.length > 0) {
    let rowsHtml = '';
    const stateNames = ['NEW', 'READY', 'RUNNING', 'YIELDED', 'COMPLETED'];
    adapt.transitions.slice(0, 15).forEach(tr => {
      rowsHtml += `
        <tr>
          <td>${tr.t.toFixed(1)}</td>
          <td><strong>${tr.name}</strong></td>
          <td>${stateNames[tr.from] || tr.from} &rarr; ${stateNames[tr.to] || tr.to}</td>
          <td>${tr.reason}</td>
        </tr>
      `;
    });
    document.getElementById('transitions-tbody').innerHTML = rowsHtml;
  }
}

function renderComparisonChart(fcfs, adapt) {
  const maxVal = Math.max(
    fcfs.avg_interactive_response_time_ms,
    adapt.avg_interactive_response_time_ms,
    fcfs.background_makespan_ms,
    adapt.background_makespan_ms,
    100.0
  );

  const h = 140;
  const w = 480;

  const f_resp_h = Math.max(4, (fcfs.avg_interactive_response_time_ms / maxVal) * 100);
  const a_resp_h = Math.max(4, (adapt.avg_interactive_response_time_ms / maxVal) * 100);
  const f_make_h = Math.max(4, (fcfs.background_makespan_ms / maxVal) * 100);
  const a_make_h = Math.max(4, (adapt.background_makespan_ms / maxVal) * 100);

  document.getElementById('comparison-chart-body').innerHTML = `
    <svg viewBox="0 0 ${w} ${h}" style="width: 100%; height: 100%;">
      <!-- Background grid lines -->
      <line x1="50" y1="20" x2="450" y2="20" stroke="#1e293b" stroke-dasharray="2" />
      <line x1="50" y1="65" x2="450" y2="65" stroke="#1e293b" stroke-dasharray="2" />
      <line x1="50" y1="110" x2="450" y2="110" stroke="#334155" />

      <!-- Group 1: Interactive Latency (Lower is Better) -->
      <rect x="90" y="${110 - f_resp_h}" width="38" height="${f_resp_h}" fill="#64748b" rx="3" />
      <text x="109" y="${105 - f_resp_h}" fill="#94a3b8" font-size="10" font-weight="bold" text-anchor="middle">${fcfs.avg_interactive_response_time_ms.toFixed(0)}ms</text>

      <rect x="134" y="${110 - a_resp_h}" width="38" height="${a_resp_h}" fill="#06b6d4" rx="3" />
      <text x="153" y="${105 - a_resp_h}" fill="#38bdf8" font-size="10" font-weight="bold" text-anchor="middle">${adapt.avg_interactive_response_time_ms.toFixed(1)}ms</text>

      <text x="131" y="126" fill="#cbd5e1" font-size="11" font-weight="bold" text-anchor="middle">Interactive Latency</text>

      <!-- Group 2: Background Makespan -->
      <rect x="290" y="${110 - f_make_h}" width="38" height="${f_make_h}" fill="#64748b" rx="3" />
      <text x="309" y="${105 - f_make_h}" fill="#94a3b8" font-size="10" font-weight="bold" text-anchor="middle">${fcfs.background_makespan_ms.toFixed(0)}ms</text>

      <rect x="334" y="${110 - a_make_h}" width="38" height="${a_make_h}" fill="#3b82f6" rx="3" />
      <text x="353" y="${105 - a_make_h}" fill="#60a5fa" font-size="10" font-weight="bold" text-anchor="middle">${adapt.background_makespan_ms.toFixed(0)}ms</text>

      <text x="331" y="126" fill="#cbd5e1" font-size="11" font-weight="bold" text-anchor="middle">Background Makespan</text>

      <!-- Legend -->
      <rect x="180" y="5" width="10" height="10" fill="#64748b" rx="2" />
      <text x="195" y="14" fill="#94a3b8" font-size="10">FCFS</text>
      <rect x="240" y="5" width="10" height="10" fill="#06b6d4" rx="2" />
      <text x="255" y="14" fill="#38bdf8" font-size="10">Adaptive</text>
    </svg>
  `;
}

function renderGanttSlices(container, slices, maxSpan, progressPct = 100) {
  if (!slices || slices.length === 0) {
    container.innerHTML = '<div class="gantt-empty-notice">No execution traces recorded.</div>';
    return;
  }

  container.innerHTML = '';
  const visibleCutoffMs = (progressPct / 100.0) * maxSpan;

  slices.forEach(sl => {
    if (sl.start > visibleCutoffMs) return;

    const sliceEnd = Math.min(sl.end, visibleCutoffMs);
    const visibleDur = Math.max(0.1, sliceEnd - sl.start);

    const leftPct = (sl.start / maxSpan) * 100;
    const widthPct = Math.max(0.6, (visibleDur / maxSpan) * 100);

    const div = document.createElement('div');
    const typeClass = sl.type === 0 ? 'slice-interactive' : (sl.type === 1 ? 'slice-compute' : 'slice-memory');
    div.className = `gantt-slice ${typeClass} ${sl.preempted ? 'slice-preempted' : ''}`;
    div.style.left = `${leftPct.toFixed(2)}%`;
    div.style.width = `${widthPct.toFixed(2)}%`;
    div.textContent = sl.name;
    div.title = `${sl.name}\nStart: ${sl.start.toFixed(1)}ms | End: ${sl.end.toFixed(1)}ms\nDuration: ${sl.dur.toFixed(1)}ms${sl.preempted ? ' (Preempted at checkpoint)' : ''}`;

    container.appendChild(div);
  });
}

// -----------------------------------------------------------------------------
// REPLAY & SCRUBBER CONTROLS
// -----------------------------------------------------------------------------
function onReplayScrub(val) {
  replayProgress = parseFloat(val);
  document.getElementById('replay-time-display').textContent = `${replayProgress.toFixed(0)}%`;

  if (lastExperimentData && lastExperimentData.fcfs && lastExperimentData.adaptive) {
    const maxSpan = Math.max(lastExperimentData.fcfs.total_benchmark_duration_ms, lastExperimentData.adaptive.total_benchmark_duration_ms, 1.0);
    renderGanttSlices(document.getElementById('fcfs-gantt-track'), lastExperimentData.fcfs.slices, maxSpan, replayProgress);
    renderGanttSlices(document.getElementById('adaptive-gantt-track'), lastExperimentData.adaptive.slices, maxSpan, replayProgress);
  }
}

function toggleReplayPlay() {
  const btn = document.getElementById('btn-replay-play');
  if (replayPlaying) {
    clearInterval(replayTimer);
    replayPlaying = false;
    btn.textContent = '⏵ Play';
  } else {
    if (replayProgress >= 100) replayProgress = 0;
    replayPlaying = true;
    btn.textContent = '⏸ Pause';

    replayTimer = setInterval(() => {
      replayProgress += (1.0 * replaySpeed);
      if (replayProgress >= 100) {
        replayProgress = 100;
        clearInterval(replayTimer);
        replayPlaying = false;
        btn.textContent = '⏵ Play';
      }
      document.getElementById('replay-time-slider').value = replayProgress;
      onReplayScrub(replayProgress);
    }, 50);
  }
}

function stepReplayForward() {
  replayProgress = Math.min(100, replayProgress + 5);
  document.getElementById('replay-time-slider').value = replayProgress;
  onReplayScrub(replayProgress);
}

function resetReplay() {
  if (replayPlaying) {
    clearInterval(replayTimer);
    replayPlaying = false;
    document.getElementById('btn-replay-play').textContent = '⏵ Play';
  }
  replayProgress = 0;
  document.getElementById('replay-time-slider').value = 0;
  onReplayScrub(0);
}

function setReplaySpeed(spd, btnEl) {
  replaySpeed = spd;
  document.querySelectorAll('.btn-speed').forEach(b => b.classList.remove('active'));
  if (btnEl) btnEl.classList.add('active');
}

// -----------------------------------------------------------------------------
// INITIALIZATION
// -----------------------------------------------------------------------------
window.addEventListener('DOMContentLoaded', () => {
  pollTelemetry();
  telemPollTimer = setInterval(pollTelemetry, 1000);
  prepareActiveWorkload();
});
