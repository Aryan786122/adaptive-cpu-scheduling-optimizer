# Technical Specification: Guided Interactive Scheduling Laboratory V3

## 1. Executive Summary & Objective

The goal is to transition the application from an automated benchmark into a **guided, educational, interactive Operating Systems laboratory**. 
A professor or student must be able to experience the complete cause-and-effect loop:
1. **Observe Real System Activity:** Collect genuine Windows telemetry (CPU utilization, top processes).
2. **Prepare the Workload:** Translate system observations into a transparent, calibrated workload model (B1, B2, I1).
3. **Watch Scheduling in Action:** Run **FCFS** first (experiencing the Convoy Effect and live queue delay, with an interactive `[+ Submit Urgent Request]` button), then run **Adaptive Priority** on the identical workload (watching cooperative checkpoints yield CPU and aging resume background work).
4. **Explain What Changed:** Analyze the microsecond Gantt traces, visual comparison bar chart, and dynamic trade-off analysis.

---

## 2. The Four Guided Stages & Backend Mapping

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ STAGE 1: OBSERVE YOUR COMPUTER                                              │
│ - Visual: Live host CPU meter, sparkline, top 4-6 processes, cores.        │
│ - Interaction: [ Capture System Activity (5s default) ]                    │
│ - Win32 Backend: Periodically reads GetProcessTimes & Toolhelp32Snapshot.   │
│ - Provenance Output: Measured duration, process count, top active PIDs,    │
│   and derived host activity load (Nominal vs Elevated).                     │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STAGE 2: PREPARE THE SCHEDULING EXPERIMENT                                  │
│ - Visual: Transparent task cards: B1 (Compute), B2 (Memory), I1 (Request).  │
│ - Controls: Mode (Captured Activity vs Standard Demo) + Intensity (Light /  │
│   Balanced [~3.5s] / Heavy).                                                │
│ - Causal Telemetry Link: Host CPU% directly governs the dynamic aging       │
│   threshold (tau = 120ms if load > 40%, else 150ms).                        │
│ - Calibration: Real mathematical iterations calibrated via QPC.             │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STAGE 3: WATCH SCHEDULING IN ACTION                                         │
│ - Step 3A: [ ▶ RUN FCFS ]                                                   │
│   * Real C threads execute on Core 0 (SetThreadAffinityMask).               │
│   * While B1 runs, user can click [ + Submit Urgent Request ]!              │
│   * Live visual alert: "Interactive task I1 waiting behind B1!"             │
│   * Live elapsed queue wait timer increments.                               │
│ - Step 3B: [ ▶ RUN ADAPTIVE ON SAME WORKLOAD ]                             │
│   * Replays identical workload and exact manual injected request offsets!   │
│   * B1 reaches cooperative preemption checkpoint (~15ms) and yields core.   │
│   * I1 executes immediately with sub-millisecond response latency.          │
│   * B1 resumes with elevated priority from aging.                           │
│ - Replay Controls: Play / Pause, Speed (0.5x, 1x, 2x), Step, Reset.        │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STAGE 4: WHAT CHANGED? (EMPIRICAL RESULTS & TRADE-OFF)                      │
│ - Visual: Dual Gantt execution traces on consistent time axis.              │
│ - Interactive hover: Inspect arrival, first dispatch, duration, wait time.  │
│ - Metric Cards: Response Latency, Queue Wait, Background Makespan.          │
│ - SVG Comparison Chart: Side-by-side bars for FCFS vs Adaptive.             │
│ - Dynamic Verdict Formula:                                                  │
│   "Adaptive scheduling reduced interactive request latency by X%,           │
│    while background completion time changed by +Y%."                        │
│ - Collapsible Technical Drawer: Raw QPC data and telemetry reasoning.       │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Data Model & REST API Additions

### New / Enhanced Endpoints in `backend/http_server.c`

1. **`POST /api/telemetry/capture`**
   - Payload: `{ "duration_seconds": 5 }`
   - Spawns Win32 background capture sampling process CPU times.
2. **`GET /api/telemetry/capture_status`**
   - Returns capture progress (`progress_percent`), `is_complete`, `observed_processes_count`, `avg_cpu_percent`, and array of `top_processes` (`pid`, `name`, `delta_cpu_ms`, `cpu_percent`).
3. **`POST /api/workload/prepare`**
   - Payload: `{ "mode": "captured" | "standard", "intensity": "light" | "balanced" | "heavy" }`
   - Generates and locks the experiment workload batch.
4. **`POST /api/experiment/run_fcfs`**
   - Runs FCFS on the prepared workload.
5. **`POST /api/experiment/inject_task`**
   - Triggered when the user clicks `[ + Submit Urgent Request ]`.
   - Records arrival timestamp $\Delta t$ relative to benchmark start and inserts the interactive task into the active workload.
6. **`POST /api/experiment/run_adaptive`**
   - Runs Adaptive Priority Scheduling on the exact same workload batch (including any injected task with identical arrival offset).
7. **`GET /api/experiment/status`**
   - Returns full status: execution state, slices, transitions, comparative metrics, and dynamic trade-off conclusion.
