/**
 * Adaptive CPU Scheduling Optimizer - Frontend Controller
 * University OS Demonstration Engine
 */

let isRunning = false;
let expPollTimer = null;
let telemPollTimer = null;

// DOM Elements - Section A
const elConnPill = document.getElementById('conn-pill');
const elConnStatusText = document.getElementById('conn-status-text');
const elCpuModel = document.getElementById('cpu-model-val');
const elCpuCores = document.getElementById('cpu-cores-val');
const elCpuUsage = document.getElementById('cpu-usage-val');
const elCpuUsageDesc = document.getElementById('cpu-usage-desc');
const elSparklinePath = document.getElementById('sparkline-path');
const elWorkloadState = document.getElementById('workload-state-val');
const elWorkloadPolicy = document.getElementById('workload-policy-val');

// DOM Elements - Section B
const elBtnRun = document.getElementById('btn-run');
const elBtnCancel = document.getElementById('btn-cancel');
const elProgressBar = document.getElementById('main-progress-bar');
const elStatusMsg = document.getElementById('stepper-status-msg');

const stepNodes = [
  document.getElementById('step-node-1'),
  document.getElementById('step-node-2'),
  document.getElementById('step-node-3'),
  document.getElementById('step-node-4'),
  document.getElementById('step-node-5')
];

const elReadyItems = document.getElementById('ready-items-container');
const elReadyBadge = document.getElementById('ready-count-badge');
const elCoreSlot = document.getElementById('core-slot-display');
const elCoreStateBadge = document.getElementById('core-state-badge');
const elCompletedItems = document.getElementById('completed-items-container');
const elCompletedBadge = document.getElementById('completed-count-badge');

// DOM Elements - Section C
const elVerdictPrimary = document.getElementById('verdict-primary-text');
const elVerdictSecondary = document.getElementById('verdict-secondary-text');

const elValFcfsResp = document.getElementById('val-fcfs-resp');
const elValAdaptResp = document.getElementById('val-adapt-resp');
const elBadgeRespPct = document.getElementById('badge-resp-pct');

const elValFcfsWait = document.getElementById('val-fcfs-wait');
const elValAdaptWait = document.getElementById('val-adapt-wait');
const elBadgeWaitPct = document.getElementById('badge-wait-pct');

const elValFcfsMake = document.getElementById('val-fcfs-make');
const elValAdaptMake = document.getElementById('val-adapt-make');
const elBadgeMakePct = document.getElementById('badge-make-pct');

const elChartBody = document.getElementById('comparison-chart-body');

const elFcfsGantt = document.getElementById('fcfs-gantt-track');
const elAdaptiveGantt = document.getElementById('adaptive-gantt-track');
const elFcfsSpanMeta = document.getElementById('fcfs-span-meta');
const elAdaptiveSpanMeta = document.getElementById('adaptive-span-meta');
const elTimelineSpanBadge = document.getElementById('timeline-span-badge');

const elAxis25 = document.getElementById('axis-label-25');
const elAxis50 = document.getElementById('axis-label-50');
const elAxis75 = document.getElementById('axis-label-75');
const elAxis100 = document.getElementById('axis-label-100');

const elTelemEvidence = document.getElementById('telem-evidence-text');
const elPreemptEvidence = document.getElementById('preemption-evidence-text');
const elTransitionsTbody = document.getElementById('transitions-tbody');

// -----------------------------------------------------------------------------
// SECTION A: REAL-TIME TELEMETRY POLLING
// -----------------------------------------------------------------------------
async function pollTelemetry() {
  try {
    const res = await fetch('/api/telemetry');
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const data = await res.json();

    // Mark online
    elConnStatusText.textContent = `Backend Connected (127.0.0.1:${window.location.port || '8000'})`;
    elConnPill.style.opacity = '1';

    // CPU Info
    elCpuModel.textContent = data.cpu_model || 'Host Windows Processor';
    elCpuCores.textContent = `${data.physical_cores || 0} Physical Cores / ${data.logical_cores || 0} Logical Processors`;

    // CPU Usage
    const cpuPct = data.cpu_percent !== undefined ? data.cpu_percent.toFixed(1) : '0.0';
    elCpuUsage.textContent = `${cpuPct}%`;
    elCpuUsageDesc.textContent = `Real-time Host Load (Active: ${data.total_threads || 0} threads)`;

    // Workload State assessment
    if (data.cpu_percent > 50.0) {
      elWorkloadState.textContent = 'Elevated Host Activity';
      elWorkloadPolicy.textContent = 'Adaptive Threshold: 120ms (Accelerated Aging)';
    } else {
      elWorkloadState.textContent = 'Nominal Host Load';
      elWorkloadPolicy.textContent = 'Adaptive Threshold: 150ms (Standard Aging)';
    }

    // Sparkline
    if (data.history && data.history.length > 1) {
      renderSparkline(data.history.map(h => h.p));
    }
  } catch (err) {
    elConnStatusText.textContent = 'Backend Reconnecting...';
    elConnPill.style.opacity = '0.6';
  }
}

function renderSparkline(points) {
  const width = 100;
  const height = 24;
  const coords = points.map((val, idx) => {
    const x = (idx / (points.length - 1)) * width;
    const y = height - (Math.min(100, Math.max(0, val)) / 100) * height;
    return `${x.toFixed(1)},${y.toFixed(1)}`;
  });
  elSparklinePath.setAttribute('d', `M ${coords.join(' L ')}`);
}

// -----------------------------------------------------------------------------
// SECTION B: EXPERIMENT EXECUTION & LIVE QUEUE ANIMATION
// -----------------------------------------------------------------------------
async function runLiveExperiment() {
  if (isRunning) return;

  isRunning = true;
  elBtnRun.disabled = true;
  elBtnCancel.disabled = false;
  elProgressBar.style.width = '5%';
  elStatusMsg.textContent = 'Dispatching hardware calibration and workload synthesizer...';

  resetStepper();
  setStep(0, 'active');

  try {
    const res = await fetch('/api/experiment/run', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ affinity_cores: 1, scale: 1.0 })
    });

    if (!res.ok) {
      const err = await res.json();
      throw new Error(err.message || 'Failed to start experiment');
    }

    if (expPollTimer) clearInterval(expPollTimer);
    expPollTimer = setInterval(pollExperimentStatus, 200);
  } catch (err) {
    elStatusMsg.textContent = `Error launching experiment: ${err.message}`;
    resetRunState();
  }
}

async function cancelLiveExperiment() {
  elStatusMsg.textContent = 'Signaling graceful experiment cancellation...';
  try {
    await fetch('/api/experiment/cancel', { method: 'POST' });
  } catch (e) {}
}

function resetRunState() {
  isRunning = false;
  elBtnRun.disabled = false;
  elBtnCancel.disabled = true;
  if (expPollTimer) {
    clearInterval(expPollTimer);
    expPollTimer = null;
  }
}

function resetStepper() {
  stepNodes.forEach(n => n.className = 'step-node');
}

function setStep(idx, state) {
  if (idx >= 0 && idx < stepNodes.length) {
    stepNodes[idx].className = `step-node ${state}`;
  }
}

async function pollExperimentStatus() {
  try {
    const res = await fetch('/api/experiment/status');
    if (!res.ok) return;
    const data = await res.json();

    // Progress bar & Message
    elProgressBar.style.width = `${data.progress_percent}%`;
    elStatusMsg.textContent = data.step_description || data.status_text;

    // Stepper updates
    // 0: IDLE, 1: CALIBRATING, 2: RUNNING_FCFS, 3: RUNNING_ADAPTIVE, 4: COMPLETED
    if (data.status_code === 1) {
      setStep(0, 'completed');
      setStep(1, 'active');
    } else if (data.status_code === 2) {
      setStep(0, 'completed');
      setStep(1, 'completed');
      setStep(2, 'active');
    } else if (data.status_code === 3) {
      setStep(0, 'completed');
      setStep(1, 'completed');
      setStep(2, 'completed');
      setStep(3, 'active');
    } else if (data.status_code === 4) {
      stepNodes.forEach(n => n.className = 'step-node completed');
    }

    // Live Queue Board Animation
    updateQueueAnimation(data);

    // Completed or Cancelled
    if (data.status_code >= 4) {
      resetRunState();
      if (data.status_code === 4) {
        displayResults(data);
      }
    }
  } catch (err) {
    console.error('Experiment poll error:', err);
  }
}

function updateQueueAnimation(data) {
  if (!data.tasks || data.tasks.length === 0) return;

  const isRunningBenchmark = (data.status_code === 2 || data.status_code === 3);
  const isFcfs = (data.status_code === 2);

  if (isRunningBenchmark) {
    elCoreStateBadge.textContent = isFcfs ? 'FCFS Dispatch' : 'Adaptive Dispatch';
    elCoreStateBadge.className = 'lane-badge badge-active';

    // Populate Ready Queue
    let readyHtml = '';
    data.tasks.forEach(t => {
      const typeLabel = t.type === 0 ? 'Interactive' : (t.type === 1 ? 'Compute' : 'Memory');
      readyHtml += `
        <div class="lane-card type-${t.type}">
          <div>
            <div class="lane-card-title">${t.name}</div>
            <div class="lane-card-meta">${typeLabel} &bull; Target: ${t.target_ms}ms</div>
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
          <span>Core 0 (Affinity Locked)</span>
          <span style="color: #60a5fa;">${isFcfs ? 'FCFS Mode' : 'Adaptive Mode'}</span>
        </div>
        <div class="core-occ-progress">
          <div class="core-occ-fill"></div>
        </div>
        <div style="font-size: 11px; color: #94a3b8; display: flex; justify-content: space-between;">
          <span>Contention Active</span>
          <span>${data.elapsed_seconds.toFixed(1)}s elapsed</span>
        </div>
      </div>
    `;
  } else if (data.status_code === 4) {
    elCoreStateBadge.textContent = 'Idle (Complete)';
    elReadyBadge.textContent = '0 Tasks';
    elReadyItems.innerHTML = '<div class="lane-empty-placeholder">All workloads executed to completion.</div>';

    elCoreSlot.innerHTML = `
      <div class="core-unoccupied" style="color: var(--color-success);">
        <span class="core-icon">✔</span>
        <span>Benchmark Run Complete &bull; All Tasks Concluded</span>
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
// SECTION C: RESULTS, EXPLANATION & GANTT VISUALIZATION
// -----------------------------------------------------------------------------
function displayResults(data) {
  const fcfs = data.fcfs;
  const adapt = data.adaptive;
  if (!fcfs || !adapt) return;

  // 1. Mandatory Format Verdict Banner
  elVerdictPrimary.textContent = data.empirical_conclusion ||
    `Adaptive scheduling reduced interactive request latency by ${data.response_time_improvement_pct.toFixed(1)}%, while background completion time changed by ${data.background_makespan_delta_pct >= 0 ? '+' : ''}${data.background_makespan_delta_pct.toFixed(1)}%.`;

  elVerdictSecondary.textContent = data.trade_off_analysis ||
    `Under non-preemptive FCFS, short interactive tasks were trapped in the ready queue behind long background computation (The Convoy Effect). The Adaptive Priority Scheduler enabled interactive requests to gain immediate CPU attention at safe cooperative checkpoints (${data.preemption_count} preemptions recorded). Dynamic aging prevented background starvation, ensuring all computational work completed reliably.`;

  // 2. Metric Cards
  // Response Latency
  elValFcfsResp.textContent = `${fcfs.avg_interactive_response_time_ms.toFixed(1)} ms`;
  elValAdaptResp.textContent = `${adapt.avg_interactive_response_time_ms.toFixed(1)} ms`;
  const respImp = data.response_time_improvement_pct;
  elBadgeRespPct.textContent = respImp >= 0 ? `+${respImp.toFixed(1)}% Faster` : `${respImp.toFixed(1)}%`;
  elBadgeRespPct.className = respImp >= 0 ? 'rm-pill-badge' : 'rm-pill-tradeoff';

  // Queue Waiting Delay
  elValFcfsWait.textContent = `${fcfs.avg_interactive_waiting_time_ms.toFixed(1)} ms`;
  elValAdaptWait.textContent = `${adapt.avg_interactive_waiting_time_ms.toFixed(1)} ms`;
  const waitImp = data.waiting_time_improvement_pct;
  elBadgeWaitPct.textContent = waitImp >= 0 ? `+${waitImp.toFixed(1)}% Less Wait` : `${waitImp.toFixed(1)}%`;
  elBadgeWaitPct.className = waitImp >= 0 ? 'rm-pill-badge' : 'rm-pill-tradeoff';

  // Background Makespan
  elValFcfsMake.textContent = `${fcfs.background_makespan_ms.toFixed(1)} ms`;
  elValAdaptMake.textContent = `${adapt.background_makespan_ms.toFixed(1)} ms`;
  const makeDelta = data.background_makespan_delta_pct;
  elBadgeMakePct.textContent = `${makeDelta >= 0 ? '+' : ''}${makeDelta.toFixed(1)}% Overhead`;

  // 3. Clear Visual Performance Comparison Chart (Native SVG)
  renderComparisonChart(fcfs, adapt);

  // 4. Gantt Execution Timelines
  const maxSpan = Math.max(fcfs.total_benchmark_duration_ms, adapt.total_benchmark_duration_ms, 1.0);
  elTimelineSpanBadge.textContent = `Total Execution Span: ${maxSpan.toFixed(1)} ms`;

  elFcfsSpanMeta.textContent = `Total Duration: ${fcfs.total_benchmark_duration_ms.toFixed(1)} ms`;
  elAdaptiveSpanMeta.textContent = `Total Duration: ${adapt.total_benchmark_duration_ms.toFixed(1)} ms &bull; ${data.preemption_count} Checkpoints`;

  elAxis25.textContent = `${(maxSpan * 0.25).toFixed(0)} ms`;
  elAxis50.textContent = `${(maxSpan * 0.50).toFixed(0)} ms`;
  elAxis75.textContent = `${(maxSpan * 0.75).toFixed(0)} ms`;
  elAxis100.textContent = `${maxSpan.toFixed(0)} ms`;

  renderGanttSlices(elFcfsGantt, fcfs.slices, maxSpan);
  renderGanttSlices(elAdaptiveGantt, adapt.slices, maxSpan);

  // 5. Collapsible Technical Details Panel
  if (data.adaptive_ctx) {
    elTelemEvidence.textContent =
      `Host CPU Load Observed:  ${data.adaptive_ctx.host_cpu.toFixed(1)}%\n` +
      `Dynamic Aging Threshold: ${data.adaptive_ctx.aging_thresh} ms\n` +
      `Cooperative Interval:    ~${data.adaptive_ctx.checkpoint_ms.toFixed(1)} ms\n` +
      `Telemetry Reasoning:     ${data.adaptive_ctx.reason}`;
  }

  elPreemptEvidence.textContent =
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
    elTransitionsTbody.innerHTML = rowsHtml;
  }
}

function renderComparisonChart(fcfs, adapt) {
  // SVG Bar Chart comparing Response Latency and Background Makespan
  const maxVal = Math.max(
    fcfs.avg_interactive_response_time_ms,
    adapt.avg_interactive_response_time_ms,
    fcfs.background_makespan_ms,
    adapt.background_makespan_ms,
    100.0
  );

  const h = 140;
  const w = 480;

  // Normalization
  const f_resp_h = Math.max(4, (fcfs.avg_interactive_response_time_ms / maxVal) * 100);
  const a_resp_h = Math.max(4, (adapt.avg_interactive_response_time_ms / maxVal) * 100);
  const f_make_h = Math.max(4, (fcfs.background_makespan_ms / maxVal) * 100);
  const a_make_h = Math.max(4, (adapt.background_makespan_ms / maxVal) * 100);

  elChartBody.innerHTML = `
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
      <rect x="240" y="5" width="10" height="10" fill="#3b82f6" rx="2" />
      <text x="255" y="14" fill="#60a5fa" font-size="10">Adaptive</text>
    </svg>
  `;
}

function renderGanttSlices(container, slices, maxSpan) {
  if (!slices || slices.length === 0) {
    container.innerHTML = '<div class="gantt-empty-notice">No execution traces recorded.</div>';
    return;
  }

  container.innerHTML = '';
  slices.forEach(sl => {
    const leftPct = (sl.start / maxSpan) * 100;
    const widthPct = Math.max(0.6, (sl.dur / maxSpan) * 100);

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
// INITIALIZATION
// -----------------------------------------------------------------------------
window.addEventListener('DOMContentLoaded', () => {
  pollTelemetry();
  telemPollTimer = setInterval(pollTelemetry, 1000);
});
