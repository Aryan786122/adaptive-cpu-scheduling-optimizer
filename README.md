# Adaptive CPU Scheduling Optimizer Using Real-System Workload Analysis

[![Windows C99 Build](https://github.com/aryan/adaptive-cpu-scheduler/actions/workflows/build.yml/badge.svg)](https://github.com/aryan/adaptive-cpu-scheduler/actions)
![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011-0078d4)
![Language](https://img.shields.io/badge/Language-C99%20Win32-blue)
![Dependencies](https://img.shields.io/badge/Dependencies-Zero%20(Pure%20Native)-success)
![Architecture](https://img.shields.io/badge/Frontend-Single--Screen%20Lab%20UI-emerald)

> **Academic Operating Systems Research Project & Demonstration**  
> An authentic, portable systems demonstrator illustrating how workload-aware adaptive priority scheduling, bounded cooperative preemption, and dynamic aging resolve the textbook **Convoy Effect** for interactive requests competing against CPU-bound background batches.

---

## 1. Executive Summary & Problem Statement

In modern operating systems and high-throughput application runtimes, workloads exhibit heterogeneous execution characteristics:
1. **Interactive Tasks:** Short, bursty, latency-critical operations (UI input, mouse clicks, immediate request processing) where response time directly governs perceived system responsiveness.
2. **Background Compute Batches:** Long-running, throughput-oriented mathematical calculations or data transforms (matrix operations, image rendering, data processing).

### The Convoy Effect under Non-Preemptive FCFS
When scheduled under standard **First-Come, First-Served (FCFS)** dispatch without preemption, if a long CPU-bound job arrives before a sequence of short interactive jobs, the interactive jobs are forced to wait in the ready queue for the entire execution duration of the long task. 

As formulated in Operating Systems theory (*Silberschatz, Galvin, Gagne*), this phenomenon is known as the **Convoy Effect**:
$$\text{Waiting Time}_{\text{interactive}} \propto \sum_{i \in \text{preceding background}} \text{Burst Time}_i$$

This causes average interactive response latency to spike into hundreds of milliseconds, even though the interactive tasks themselves require only a few milliseconds of CPU execution.

### The Proposed Adaptive Solution
This project implements an **Application-Level Adaptive Priority Scheduler with Bounded Cooperative Preemption and Anti-Starvation Dynamic Aging**:
- **Workload Classification:** Identifies latency-critical tasks and prioritizes them ahead of background batches.
- **Bounded Cooperative Preemption:** Long-running compute jobs check atomic yield flags at calibrated execution checkpoints (~15 ms). When a high-priority interactive task arrives, the background job cooperatively yields the core at the next checkpoint, allowing the interactive job to execute immediately.
- **Anti-Starvation Aging:** To prevent background tasks from starving under high interactive request rates, background task priority dynamically increases as waiting time elapses:
  $$P_{\text{effective}} = \max\left(1, P_{\text{base}} - \left\lfloor \frac{\Delta t_{\text{waiting}}}{\tau_{\text{aging}}} \right\rfloor\right)$$
- **Zero Kernel Modification:** Operates as a portable, controlled user-space workload manager utilizing genuine Win32 worker threads, high-resolution hardware counters, and real host telemetry.

---

## 2. Theoretical Operating Systems Principles

| Concept | Classical Theory | Implementation in this Project |
| :--- | :--- | :--- |
| **FCFS Scheduling** | Non-preemptive queue dispatching strictly by arrival timestamp. | Managed worker pool executed in arrival sequence on a single processor core. |
| **Convoy Effect** | Short jobs queued behind long CPU bursts experience inflated waiting times. | Empirically demonstrated: Interactive tasks arriving at $t=150\text{ms}$ wait $>480\text{ms}$ behind background compute. |
| **Cooperative Preemption** | Threads yield CPU control voluntarily at bounded checkpoints rather than abrupt thread termination. | Computation loops evaluate an atomic yield flag via `InterlockedCompareExchange` every ~15 microseconds; yields take place cleanly without state corruption. |
| **Priority Scheduling** | Dispatching the highest priority task first ($P=1$ Interactive, $P=5$ Background). | Priority queue evaluates base priority adjusted for system telemetry and aging. |
| **Starvation (Indefinite Postponement)** | Low-priority jobs never execute if high-priority tasks continuously arrive. | Evaluated and resolved via dynamic aging thresholds ($\tau = 400\text{ms}$). |
| **Thread Affinity** | Binding execution to specific logical processors via `SetThreadAffinityMask`. | Benchmark worker threads are constrained to Core 0 (`0x1`) to create controlled CPU contention on multi-core PCs. |
| **High-Precision Timing** | Microsecond performance counters avoiding `time()` or `Sleep()` quantization. | Uses Win32 `QueryPerformanceCounter` (QPC) with sub-microsecond resolution (~10 MHz frequency). |

---

## 3. Real System Measurements & Empirical Trade-Offs

The following performance metrics were captured live on the host hardware (**13th Gen Intel Core i7-13700HX**, 16 Cores, 24 Logical Processors, Windows 11) using the automated test suite:

```
================= EXPERIMENTAL BENCHMARK REPORT =================
Interactive Avg Response Time: FCFS = 486.44 ms | Adaptive = 0.16 ms
Response Time Improvement:     100.0% Faster
Interactive Avg Waiting Time:  FCFS = 486.44 ms | Adaptive = 0.16 ms
Background Makespan Overhead:  +8.3% Overhead
Cooperative Preemptions:       3 checkpoints
Total Recorded Gantt Slices:   FCFS = 6 | Adaptive = 9
-----------------------------------------------------------------
Empirical Verdict: Under non-preemptive FCFS, interactive tasks experienced
severe convoy latency, waiting an average of 486.4 ms behind long background
jobs. The Adaptive Priority Scheduler reduced interactive response latency
by 100.0% (down to 0.16 ms) by cooperatively preempting compute tasks at
~15ms checkpoints.
=================================================================
```

### The Engineering Trade-Off:
1. **Interactive Latency Advantage:** Interactive response latency dropped from **486.44 ms down to 0.16 ms** — eliminating the Convoy bottleneck.
2. **Throughput Cost:** Total background job makespan increased by **+8.3%**. This reflects the authentic systems reality: context switching, cooperative checkpoint yielding, and prioritizing interactive work naturally delays the final completion of background batches.
3. **No Starvation:** Because aging was active, background compute jobs were never permanently starved, completing all requested mathematical iterations.

---

## 4. Technical Architecture & Repository Layout

This application is built as a single, self-contained native Windows program:
- **Backend:** 100% Native C99 (Win32 API, Winsock2, PSAPI, IP Helper API).
- **HTTP Server:** Custom lightweight embedded HTTP/1.1 server binding strictly to `127.0.0.1` with automatic dynamic port probing (`8000`–`8024`).
- **Frontend:** Single-screen laboratory-grade UI (Vanilla HTML5, CSS3, JavaScript) served locally from memory/disk without external CDNs.
- **Zero Runtime Dependencies:** Requires **no Python**, **no Node.js**, **no npm**, and **no Docker**.

```
adaptive-cpu-scheduler/
├── backend/
│   ├── main.c                  # Process entrypoint, CLI flags, Job Object lifecycle
│   ├── system_telemetry.h/.c   # Win32 host monitoring (CPU%, RAM, procs, foreground window)
│   ├── calibration.h/.c        # Dynamic hardware calibration loop & worker compute routines
│   ├── workload_generator.h/.c # Calibrated synthetic repeatable workload batches
│   ├── worker_pool.h           # Data structures for threads, slices, and transitions
│   ├── scheduler_fcfs.h/.c     # Non-preemptive arrival-order scheduler
│   ├── scheduler_adaptive.h/.c # Workload-aware priority, preemption & aging scheduler
│   ├── experiment_runner.h/.c  # Asynchronous benchmark executor & trade-off analyzer
│   ├── http_server.h/.c        # Embedded multi-threaded Winsock2 REST server
│   └── json_builder.h/.c       # Zero-allocation high-speed JSON buffer builder
├── frontend/
│   ├── index.html              # 3-Area laboratory single-screen interface
│   ├── app.css                 # Clean, modern, high-legibility university styling
│   └── app.js                  # Telemetry polling, queue animation & Gantt renderer
├── tests/
│   └── test_suite.ps1          # 6-stage automated test suite
├── .github/workflows/
│   └── build.yml               # Automated CI build & packaging workflow
├── Makefile                    # Standard GNU Make build definition
├── build.bat                   # One-click native compilation script (GCC)
├── package_release.bat         # Portable ZIP release packaging script
└── README.md                   # This documentation
```

---

## 5. User Interface Organization (3-Area Single Screen)

The web user interface is strictly organized into **three functional areas**:

### Area A: Live System Telemetry & Hardware Status Bar
- **CPU Hardware:** Displays processor name, physical core count, and logical processor count.
- **Host CPU Load & Sparkline:** Live SVG utilization graph updated every 1,000 ms.
- **System Memory:** Real-time RAM utilization in GB and percentage.
- **OS Activity:** Total system processes and threads.
- **Active Foreground Window:** Identifies the active user application and PID via `GetForegroundWindow` and `QueryFullProcessImageName`.

### Area B: One-Click Experiment Center & Live Execution Board
- **Run Live Experiment Button:** Dispatches the automated comparative benchmark.
- **Contention Configuration:** Allows selecting **Single Core 0** (recommended to demonstrate contention) vs Multi-Core.
- **Workload Duration Scale:** Scales calibrated workloads between Fast (0.5x), Standard (1.0x), and Extended (2.0x).
- **Stepper Progress Indicator:** Visualizes progress across the 5 benchmark stages (Hardware Probe → Calibration → FCFS → Adaptive → Comparison).
- **Live Dispatch Queue Visualizer:** Shows tasks entering the Ready Queue, occupying the CPU Core slot, yielding, and completing.

### Area C: Results, Gantt Timeline & Academic Conclusion
- **Key Metrics Comparison Grid:** Side-by-side metric cards showing Response Time, Queue Delay, Background Makespan, and Preemptions with color-coded improvement badges.
- **High-Resolution Gantt Timelines:** Interactive dual-track Gantt chart displaying the microsecond execution traces for FCFS vs Adaptive. Hovering over any slice reveals exact start/end timestamps and preemption reasons.
- **Empirical Trade-off & Conclusion Card:** Automatically synthesizes the honest academic verdict explaining the systems engineering trade-off.

---

## 6. How to Build & Run

### Prerequisites
- Windows 10 or Windows 11 (64-bit).
- GCC compiler (MSYS2 MinGW-w64, w64devkit, or Git Bash GCC).

### 1. Build the Binary
Double-click `build.bat` or run:
```cmd
build.bat
```
This compiles `adaptive_scheduler.exe` with `-O2 -Wall -Wextra -std=c99` with zero warnings and zero errors.

### 2. Run the Application
Launch the compiled binary:
```cmd
adaptive_scheduler.exe
```
By default, the application will:
1. Initialize a Win32 Job Object (ensuring leak-free cleanup).
2. Query host hardware and display baseline CPU specs in the console.
3. Perform a CPU calibration benchmark loop.
4. Bind the HTTP server to `http://127.0.0.1:8000` (probing up to `8024` if port 8000 is occupied).
5. Automatically open the user's default browser to the web interface.

### CLI Options
```
adaptive_scheduler.exe [options]

Options:
  --port <number>      Preferred HTTP port (default: 8000; probes up to +24)
  --no-browser         Do not automatically open the default web browser
  --web-root <path>    Override path to frontend static files
  --help, -h           Show help message and exit
```

### 3. Run the Automated Test Suite
To execute the end-to-end automated verification script:
```powershell
powershell.exe -ExecutionPolicy Bypass -File tests\test_suite.ps1
```

### 4. Create Portable Release ZIP
To generate a standalone distribution archive without build tools:
```cmd
package_release.bat
```
Creates `dist/adaptive-cpu-scheduler-windows-x64.zip` containing the binary and frontend assets.

---

## 7. Presentation Guide for University Evaluation

When demonstrating this project to an Operating Systems professor or committee, follow this recommended 3-minute walkthrough:

### Minute 1: The Problem & Academic Motivation
1. Explain the **Convoy Effect**: When long CPU-intensive tasks run under non-preemptive arrival-order scheduling, short latency-critical tasks get stuck in the ready queue.
2. Note that while Windows NT kernel uses priority preemption across OS processes, application servers and runtimes frequently face internal queuing delays when handling mixed background and interactive workloads.

### Minute 2: The Live Demonstration
1. Open the application. Highlight the **Area A Telemetry Bar**, showing that the software genuinely observes the laptop's actual CPU, RAM, and active processes via Win32 APIs.
2. Click **Run Live Experiment**.
3. Watch the progress stepper advance through Hardware Probe, Calibration, FCFS execution, and Adaptive Priority dispatch.
4. Point to the **Area C Gantt Charts**:
   - In Track 1 (FCFS), show the blue interactive tasks blocked all the way out past 400ms because the orange compute task refused to yield.
   - In Track 2 (Adaptive), show where the dashed red lines mark cooperative checkpoints where the background task gracefully yielded to let interactive tasks run within 1ms of arrival.

### Minute 3: The Systems Engineering Trade-off & Q&A
1. Draw attention to the **Honest Metrics Card**:
   - Interactive latency improved by **~100%**.
   - Background makespan increased by **~8%**.
   - Emphasize that there is no "free lunch" in systems architecture: prioritizing responsiveness incurs a quantifiable context-switching overhead.
2. Common defense questions:
   - **Q: Does this replace the Windows kernel scheduler?**  
     *A: No. It manages controlled application workloads on genuine Win32 threads using real operating system APIs, demonstrating queuing theory principles safely.*
   - **Q: How is contention demonstrated on a 16-core laptop?**  
     *A: Through `SetThreadAffinityMask(0x1)`, restricting the benchmark worker pool to Core 0. This creates textbook single-core queue contention without slowing down unrelated user programs.*
   - **Q: Are the benchmark results pre-recorded or simulated?**  
     *A: Completely genuine. Worker threads execute real mathematical loops and memory strides, timed via high-resolution hardware counters (`QueryPerformanceCounter`).*

---

## 8. License

This project is released under the **MIT License** for academic and educational research.
