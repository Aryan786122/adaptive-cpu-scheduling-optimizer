import sys
import os
import pptx
from pptx.util import Inches, Pt
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE

def create_presentation():
    prs = pptx.Presentation()
    prs.slide_width = Inches(13.333)
    prs.slide_height = Inches(7.5)
    blank_layout = prs.slide_layouts[6]

    # Theme Colors
    BG_COLOR = RGBColor(11, 17, 32)         # #0B1120 Dark Navy
    CARD_BG = RGBColor(24, 33, 54)          # #182136 Card Fill
    CARD_BORDER = RGBColor(51, 65, 85)      # #334155 Slate Border
    ACCENT_CYAN = RGBColor(56, 189, 248)    # #38BDF8 Bright Cyan
    ACCENT_GREEN = RGBColor(16, 185, 129)   # #10B981 Emerald
    ACCENT_AMBER = RGBColor(245, 158, 11)   # #F59E0B Warning Amber
    TEXT_LIGHT = RGBColor(248, 250, 252)    # #F8FAFC Off-white
    TEXT_DIM = RGBColor(148, 163, 184)      # #94A3B8 Muted slate
    TEXT_DARK = RGBColor(15, 23, 42)

    def set_slide_background(slide):
        bg = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, Inches(13.333), Inches(7.5))
        bg.fill.solid()
        bg.fill.fore_color.rgb = BG_COLOR
        bg.line.fill.background() # no border

    def add_header(slide, category_text, title_text, subtitle_text=""):
        # Category Badge
        cat_box = slide.shapes.add_textbox(Inches(0.8), Inches(0.4), Inches(8), Inches(0.35))
        tf = cat_box.text_frame
        tf.word_wrap = True
        p = tf.paragraphs[0]
        p.text = category_text.upper()
        p.font.size = Pt(10)
        p.font.bold = True
        p.font.color.rgb = ACCENT_CYAN
        
        # Title
        title_box = slide.shapes.add_textbox(Inches(0.8), Inches(0.7), Inches(11.7), Inches(0.6))
        tf2 = title_box.text_frame
        tf2.word_wrap = True
        p2 = tf2.paragraphs[0]
        p2.text = title_text
        p2.font.size = Pt(22)
        p2.font.bold = True
        p2.font.color.rgb = TEXT_LIGHT

        # Subtitle
        if subtitle_text:
            sub_box = slide.shapes.add_textbox(Inches(0.8), Inches(1.25), Inches(11.7), Inches(0.4))
            tf3 = sub_box.text_frame
            tf3.word_wrap = True
            p3 = tf3.paragraphs[0]
            p3.text = subtitle_text
            p3.font.size = Pt(12)
            p3.font.color.rgb = TEXT_DIM

    def add_card(slide, left, top, width, height, title, items, border_color=CARD_BORDER, header_color=ACCENT_CYAN):
        # Card Background Box
        card = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(left), Inches(top), Inches(width), Inches(height))
        card.fill.solid()
        card.fill.fore_color.rgb = CARD_BG
        card.line.color.rgb = border_color
        card.line.width = Pt(1)

        # Card Content
        tb = slide.shapes.add_textbox(Inches(left + 0.25), Inches(top + 0.2), Inches(width - 0.5), Inches(height - 0.4))
        tf = tb.text_frame
        tf.word_wrap = True

        # Title
        p0 = tf.paragraphs[0]
        p0.text = title
        p0.font.size = Pt(14)
        p0.font.bold = True
        p0.font.color.rgb = header_color
        p0.space_after = Pt(10)

        # Items
        for item in items:
            p = tf.add_paragraph()
            p.text = "• " + item
            p.font.size = Pt(11)
            p.font.color.rgb = TEXT_LIGHT
            p.space_after = Pt(6)

    # =========================================================================
    # SLIDE 1: TITLE SLIDE
    # =========================================================================
    s1 = prs.slides.add_slide(blank_layout)
    set_slide_background(s1)

    # Decorative header pill
    pill = s1.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.8), Inches(1.2), Inches(3.2), Inches(0.35))
    pill.fill.solid()
    pill.fill.fore_color.rgb = RGBColor(14, 165, 233)
    pill.line.fill.background()
    p_tf = pill.text_frame
    p_p = p_tf.paragraphs[0]
    p_p.text = "OPERATING SYSTEMS RESEARCH PROJECT"
    p_p.font.size = Pt(9)
    p_p.font.bold = True
    p_p.alignment = PP_ALIGN.CENTER
    p_p.font.color.rgb = TEXT_LIGHT

    # Main Title
    t_box = s1.shapes.add_textbox(Inches(0.8), Inches(1.7), Inches(11.7), Inches(1.8))
    tf = t_box.text_frame
    tf.word_wrap = True
    p1 = tf.paragraphs[0]
    p1.text = "Adaptive CPU Scheduling Optimizer"
    p1.font.size = Pt(36)
    p1.font.bold = True
    p1.font.color.rgb = TEXT_LIGHT
    
    p2 = tf.add_paragraph()
    p2.text = "Using Real-System Workload Analysis on Windows NT"
    p2.font.size = Pt(24)
    p2.font.bold = True
    p2.font.color.rgb = ACCENT_CYAN

    # Subtitle / Abstract
    desc_box = s1.shapes.add_textbox(Inches(0.8), Inches(3.6), Inches(11.5), Inches(1.2))
    d_tf = desc_box.text_frame
    d_tf.word_wrap = True
    dp = d_tf.paragraphs[0]
    dp.text = "An empirical demonstration comparing First-Come, First-Served (FCFS) Baseline vs. Workload-Aware Priority Modulation with Dynamic Aging Watchdogs under hardware-saturating CPU contention and multi-tab browser workloads."
    dp.font.size = Pt(14)
    dp.font.color.rgb = TEXT_DIM

    # Highlights Grid (3 chips at bottom)
    chips = [
        ("Core Technology", "Native Win32 C99 Engine (Zero Python/Node dependencies on target)", ACCENT_CYAN),
        ("Hardware Grounding", "QueryPerformanceCounter (<1µs precision) & Live Win32 Telemetry", ACCENT_GREEN),
        ("Real Application Workload", "Automated 10-Tab Google Chrome / Microsoft Edge Contention", ACCENT_AMBER),
    ]
    for idx, (head, val, col) in enumerate(chips):
        c_left = 0.8 + idx * 3.95
        card = s1.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(c_left), Inches(5.1), Inches(3.8), Inches(1.5))
        card.fill.solid()
        card.fill.fore_color.rgb = CARD_BG
        card.line.color.rgb = CARD_BORDER
        card.line.width = Pt(1)
        
        ctb = s1.shapes.add_textbox(Inches(c_left + 0.2), Inches(5.2), Inches(3.4), Inches(1.3))
        ctf = ctb.text_frame
        ctf.word_wrap = True
        cp1 = ctf.paragraphs[0]
        cp1.text = head
        cp1.font.size = Pt(11)
        cp1.font.bold = True
        cp1.font.color.rgb = col
        cp1.space_after = Pt(4)
        cp2 = ctf.add_paragraph()
        cp2.text = val
        cp2.font.size = Pt(10)
        cp2.font.color.rgb = TEXT_LIGHT

    # =========================================================================
    # SLIDE 2: MOTIVATION & PROBLEM STATEMENT
    # =========================================================================
    s2 = prs.slides.add_slide(blank_layout)
    set_slide_background(s2)
    add_header(s2, "Problem Formulation", "The Challenge: Interactive Responsiveness Under CPU Contention",
               "Why standard non-preemptive or equal-share scheduling causes severe user experience degradation.")

    add_card(s2, 0.8, 1.8, 5.7, 5.0, "The Real-World Bottleneck: The Convoy Effect", [
        "Modern desktops concurrently run long batch operations (cryptographic hashing, data analytics, media rendering) alongside latency-critical interactive applications (web browsers, GUIs).",
        "Under naive First-Come, First-Served (FCFS) or equal-share round-robin scheduling, short interactive tasks are placed at the rear of ready queues.",
        "Interactive requests must wait for long compute-bound jobs to exhaust their CPU quanta (Head-of-Line Blocking).",
        "Consequence: Dropped display frames, input lag, unresponsive UI, and degraded user satisfaction despite multi-core modern hardware.",
    ], CARD_BORDER, ACCENT_AMBER)

    add_card(s2, 6.8, 1.8, 5.7, 5.0, "Research Objectives & Project Scope", [
        "Design and implement a workload-aware user-space scheduling optimizer targeting modern multi-core Windows architectures.",
        "Ground scheduling decisions in live Win32 system telemetry (host CPU load, active threads, available cores).",
        "Demonstrate cooperative preemption and dynamic Win32 thread priority modulation (Normal vs Lowest).",
        "Integrate an Aging Watchdog mechanism to mathematically eliminate starvation of background jobs.",
        "Validate results empirically through both a theoretical synthetic lab and an automated 10-tab live Google Chrome benchmark.",
    ], CARD_BORDER, ACCENT_CYAN)

    # =========================================================================
    # SLIDE 3: OPERATING SYSTEMS THEORY & CONCEPTS
    # =========================================================================
    s3 = prs.slides.add_slide(blank_layout)
    set_slide_background(s3)
    add_header(s3, "Theoretical Foundations", "Core Operating System Scheduling Principles",
               "The mathematical and algorithmic tenets implemented and visualized in this system.")

    add_card(s3, 0.8, 1.8, 3.7, 5.0, "1. FCFS & Head-of-Line Blocking", [
        "Jobs are dispatched strictly in arrival sequence without interruption.",
        "Algorithm complexity is minimal: O(1) queue insertion and removal.",
        "Zero scheduling overhead and excellent hardware cache locality.",
        "Critical weakness: Severe variance in waiting time (Convoy Effect) when long burst tasks arrive prior to short interactive tasks.",
    ], CARD_BORDER, ACCENT_AMBER)

    add_card(s3, 4.8, 1.8, 3.7, 5.0, "2. Priority & Preemption", [
        "Tasks are classified into priority tiers based on burst profile and interactivity.",
        "Interactive tasks receive elevated priority; computational batch jobs are demoted.",
        "Preemptive Dispatch: The scheduler interrupts executing low-priority threads the microsecond a high-priority task arrives.",
        "Slashes interactive queue wait time by up to 90%.",
    ], CARD_BORDER, ACCENT_CYAN)

    add_card(s3, 8.8, 1.8, 3.7, 5.0, "3. Dynamic Aging & Starvation", [
        "Fundamental risk of pure priority scheduling: Starvation (indefinite blocking of batch tasks).",
        "Solution: Dynamic Aging Watchdog.",
        "As a low-priority thread waits in the ready queue, its effective priority increases monotonically with time.",
        "In our implementation: Threshold of 1,200ms triggers a temporary boost to guarantee forward execution progress.",
    ], CARD_BORDER, ACCENT_GREEN)

    # =========================================================================
    # SLIDE 4: ARCHITECTURE & IMPLEMENTATION
    # =========================================================================
    s4 = prs.slides.add_slide(blank_layout)
    set_slide_background(s4)
    add_header(s4, "Engineering Design", "Native Win32 C99 System Architecture",
               "Lightweight, dependency-free implementation interacting directly with Windows NT kernel APIs.")

    add_card(s4, 0.8, 1.8, 5.7, 2.4, "High-Precision Hardware Timing", [
        "Utilizes Windows QueryPerformanceCounter (QPC) and QueryPerformanceFrequency.",
        "Sub-microsecond timer resolution (~10 MHz clock frequency).",
        "Accurately measures arrival delays, CPU service times, and queue latency with zero simulated delays.",
    ], CARD_BORDER, ACCENT_CYAN)

    add_card(s4, 0.8, 4.4, 5.7, 2.4, "Kernel Interfacing & Thread Modulation", [
        "Uses Win32 SetThreadPriority to modulate worker priorities (NORMAL level 8 vs LOWEST level 4).",
        "Leak-proof process lifecycle managed via Win32 Job Objects (JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE).",
        "Hardware-aware concurrency: automatically detects and saturates all host logical processors.",
    ], CARD_BORDER, ACCENT_GREEN)

    add_card(s4, 6.8, 1.8, 5.7, 5.0, "Full-Stack Embedded Web Architecture", [
        "Zero Runtime Dependencies: Fully written in standard ISO C99; zero Python, Node.js, or external web servers required on target PC.",
        "Embedded Win32 Winsock HTTP Server: High-performance non-blocking socket loop serving static web UI and REST JSON endpoints.",
        "RESTful Control Endpoints:",
        "  • /api/telemetry — Host hardware specs & live CPU load",
        "  • /api/experiment/run_fcfs & run_adaptive — Theory lab execution",
        "  • /api/real_browser/start & status — Live browser benchmark orchestration",
        "Single-Screen Responsive Dashboard: Built with native HTML5 Canvas, modern CSS, and JavaScript event loops.",
    ], CARD_BORDER, ACCENT_CYAN)

    # =========================================================================
    # SLIDE 5: EXPERIMENT 1 — REAL BROWSER BENCHMARK
    # =========================================================================
    s5 = prs.slides.add_slide(blank_layout)
    set_slide_background(s5)
    add_header(s5, "Real-World Benchmark", "Experiment 1: 10-Tab Browser Contention Test",
               "Evaluating real browser responsiveness during hardware-saturating background cryptographic hashing.")

    add_card(s5, 0.8, 1.8, 3.7, 5.0, "1. Background CPU Contention", [
        "Native C engine spawns concurrent worker threads matching host logical cores (e.g. 24 threads on Intel i7-13700HX).",
        "Workers execute continuous loops of pseudo-SHA-256 and matrix multiplication.",
        "Zero sleep calls: saturates all cores to generate genuine, continuous CPU contention.",
        "Tracks completed hash rounds into the billions.",
    ], CARD_BORDER, ACCENT_AMBER)

    add_card(s5, 4.8, 1.8, 3.7, 5.0, "2. Isolated Multi-Tab Launch", [
        "Spawns Google Chrome or Microsoft Edge in an isolated temporary profile (%TEMP%\\sched_browser_test_*).",
        "Completely safe: User's personal browser profile, cookies, and history are never accessed.",
        "Opens 10 concurrent tabs running intensive client-side DOM and Canvas workloads.",
        "Win32 Job Object automatically terminates only test tabs on completion.",
    ], CARD_BORDER, ACCENT_CYAN)

    add_card(s5, 8.8, 1.8, 3.7, 5.0, "3. In-Tab Telemetry Measurement", [
        "DOM Batching: 1,500 styled DOM items generated across 25 frames.",
        "Active 2D Canvas Rasterization: Multi-frame trigonometric wave rendering.",
        "Event Loop Queue Delay: Measures actual vs scheduled setTimeout(50ms) timestamps to quantify main-thread UI freezing.",
        "Automated telemetry feedback: Tabs report load time and event delay via REST JSON.",
    ], CARD_BORDER, ACCENT_GREEN)

    # =========================================================================
    # SLIDE 6: POLICY A VS POLICY B MECHANICS
    # =========================================================================
    s6 = prs.slides.add_slide(blank_layout)
    set_slide_background(s6)
    add_header(s6, "Policy Comparison", "Scheduling Mechanics: FCFS vs Adaptive Priority",
               "How the Windows NT dispatcher handles thread quanta under each experimental policy.")

    add_card(s6, 0.8, 1.8, 5.7, 5.0, "Policy A: FCFS Baseline (Equal Share)", [
        "Configuration: Background hashing threads run at THREAD_PRIORITY_NORMAL (Windows base priority 8).",
        "Kernel Treatment: The Windows NT scheduler treats background compute threads and Chrome renderer threads with identical priority.",
        "Execution Behavior: Chrome renderer threads must wait in line for background threads to exhaust their time slices (quanta).",
        "Visual Impact: Red/Orange progress bars creep slowly across the screen with visible stutter.",
        "Measured Outcome: ~11,564 ms average tab completion time under heavy contention.",
    ], CARD_BORDER, ACCENT_AMBER)

    add_card(s6, 6.8, 1.8, 5.7, 5.0, "Policy B: Adaptive Priority + Aging Watchdog", [
        "Initial Demotion: Background workers are immediately demoted to THREAD_PRIORITY_LOWEST (Windows base priority 4).",
        "Preemptive Execution: Windows scheduler prioritizes Chrome threads (levels 8-10), granting them instant CPU quanta without queue waiting.",
        "Aging Watchdog: Dedicated scheduler thread tracks background starvation; triggers a 160ms boost to NORMAL priority every 1,200ms.",
        "Visual Impact: Vibrant cyan progress bars rush forward smoothly; priority badges actively modulate.",
        "Measured Outcome: ~5,448 ms average tab completion time (52.9% Faster!).",
    ], CARD_BORDER, ACCENT_GREEN)

    # =========================================================================
    # SLIDE 7: EMPIRICAL BENCHMARK RESULTS
    # =========================================================================
    s7 = prs.slides.add_slide(blank_layout)
    set_slide_background(s7)
    add_header(s7, "Experimental Results", "Quantitative Findings: Real Browser Benchmark",
               "Measured metrics collected across 10 concurrent tabs on an Intel Core i7-13700HX (24 Logical Processors).")

    # Table of Results
    rows = 4
    cols = 4
    table_shape = s7.shapes.add_table(rows, cols, Inches(0.8), Inches(1.8), Inches(11.7), Inches(2.2))
    table = table_shape.table

    table_data = [
        ["Performance Metric", "Policy A: FCFS Baseline", "Policy B: Adaptive Priority", "Empirical Improvement"],
        ["Average Tab Load Latency", "11,564.3 ms", "5,448.3 ms", "52.9% Faster Page Load (✔)"],
        ["Main-Thread Event Loop Lag", "98.2 ms delay", "85.9 ms delay", "12.6% Lower Event Delay (✔)"],
        ["Background Execution Makespan", "10,258.8 ms", "5,833.7 ms", "Overhead Absorbed via Idle Quanta"],
    ]

    for r_idx, row in enumerate(table_data):
        for c_idx, val in enumerate(row):
            cell = table.cell(r_idx, c_idx)
            cell.text = val
            cell.fill.solid()
            if r_idx == 0:
                cell.fill.fore_color.rgb = RGBColor(30, 41, 59)
                p = cell.text_frame.paragraphs[0]
                p.font.bold = True
                p.font.color.rgb = ACCENT_CYAN
                p.font.size = Pt(11)
            else:
                cell.fill.fore_color.rgb = CARD_BG
                p = cell.text_frame.paragraphs[0]
                p.font.size = Pt(11)
                p.font.color.rgb = TEXT_LIGHT
                if c_idx == 3:
                    p.font.bold = True
                    p.font.color.rgb = ACCENT_GREEN

    # Key Findings Cards below table
    add_card(s7, 0.8, 4.3, 5.7, 2.6, "Visual Bar Chart Representation", [
        "Dashboard features a Gantt-style horizontal timeline comparison bar.",
        "FCFS bar stretches across 100% of width (11,564ms) showing queue stalling.",
        "Adaptive bar is cut in half (5,448ms) showing instant preemptive execution.",
        "Gives professors and evaluators instant visual verification of results.",
    ], CARD_BORDER, ACCENT_CYAN)

    add_card(s7, 6.8, 4.3, 5.7, 2.6, "Academic Integrity & Dynamic Evaluation", [
        "Results are computed dynamically from actual millisecond deltas.",
        "The system never hardcodes winners: if FCFS performs better under low contention, the UI truthfully declares FCFS the winner with amber badges.",
        "Under true core saturation, Adaptive consistently dominates interactive responsiveness.",
    ], CARD_BORDER, ACCENT_GREEN)

    # =========================================================================
    # SLIDE 8: EXPERIMENT 2 — 4-STAGE OS THEORY LAB
    # =========================================================================
    s8 = prs.slides.add_slide(blank_layout)
    set_slide_background(s8)
    add_header(s8, "Educational Demonstrator", "Experiment 2: 4-Stage Guided OS Theory Lab",
               "An interactive pedagogical laboratory demonstrating synthetic task queue dynamics and preemption.")

    stages = [
        ("STAGE 1: CAPTURE", "Samples live Windows host CPU utilization via NtQuerySystemInformation to ground experiment parameters in actual PC load.", ACCENT_CYAN),
        ("STAGE 2: PREPARE", "Derives a calibrated synthetic task workload mix: long background compute batches (B1, B2) and latency-sensitive interactive requests (I1).", ACCENT_GREEN),
        ("STAGE 3: OBSERVE", "Executes FCFS vs Adaptive live. Allows real-time injection of urgent tasks to visibly demonstrate Head-of-Line Blocking vs Preemption.", ACCENT_AMBER),
        ("STAGE 4: REPLAY", "Step-by-step interactive timeline playback with Play/Pause controls, dual Gantt charts, and microsecond turnaround breakdowns.", RGBColor(168, 85, 247)),
    ]

    for idx, (st_title, st_desc, st_col) in enumerate(stages):
        top_pos = 1.8 + idx * 1.25
        card = s8.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.8), Inches(top_pos), Inches(11.7), Inches(1.1))
        card.fill.solid()
        card.fill.fore_color.rgb = CARD_BG
        card.line.color.rgb = CARD_BORDER
        card.line.width = Pt(1)

        tb = s8.shapes.add_textbox(Inches(1.0), Inches(top_pos + 0.15), Inches(11.3), Inches(0.8))
        tf = tb.text_frame
        tf.word_wrap = True
        p1 = tf.paragraphs[0]
        p1.text = st_title
        p1.font.bold = True
        p1.font.size = Pt(13)
        p1.font.color.rgb = st_col
        p1.space_after = Pt(2)
        p2 = tf.add_paragraph()
        p2.text = st_desc
        p2.font.size = Pt(11)
        p2.font.color.rgb = TEXT_LIGHT

    # =========================================================================
    # SLIDE 9: THE FUNDAMENTAL OS TRADE-OFF
    # =========================================================================
    s9 = prs.slides.add_slide(blank_layout)
    set_slide_background(s9)
    add_header(s9, "Theoretical Insight", "The Fundamental Operating System Trade-Off",
               "Why no single scheduling algorithm is universally superior under all operating conditions.")

    add_card(s9, 0.8, 1.8, 5.7, 5.0, "When FCFS is Optimal (Low Contention)", [
        "Zero Algorithm Overhead: FCFS incurs O(1) scheduling logic with zero kernel-mode context switches.",
        "Superior Cache Locality: Threads remain warm on their allocated physical cores without thread migration.",
        "Zero System Call Penalties: No periodic SetThreadPriority or watchdog timer interrupts.",
        "When CPU cores have idle capacity (core slack), FCFS matches or slightly beats Adaptive because priority management overhead is avoided.",
    ], CARD_BORDER, ACCENT_AMBER)

    add_card(s9, 6.8, 1.8, 5.7, 5.0, "When Adaptive is Vital (High Saturation)", [
        "Preemptive Guarantee: Prevents heavy compute jobs from holding the CPU hostage.",
        "Interactive Latency Slashed: Reduces browser load times by >50% and eliminates UI freezing.",
        "Acceptable Trade-off: Background computation experiences a minor makespan penalty in exchange for human UI responsiveness.",
        "Starvation Immunity: Dynamic aging ensures long batch tasks always achieve forward progress.",
        "Reflects the architectural philosophy of modern production schedulers (Linux CFS, Windows NT Priority Bands).",
    ], CARD_BORDER, ACCENT_GREEN)

    # =========================================================================
    # SLIDE 10: CONCLUSION & SUMMARY
    # =========================================================================
    s10 = prs.slides.add_slide(blank_layout)
    set_slide_background(s10)
    add_header(s10, "Summary & Conclusion", "Key Takeaways & Academic Deliverables",
               "Demonstrated results, engineering accomplishments, and standalone portability.")

    add_card(s10, 0.8, 1.8, 5.7, 5.0, "Project Accomplishments", [
        "1. Full-Stack Native Implementation: C99 backend + embedded HTTP server + live browser frontend with zero external dependencies.",
        "2. Real Application Validation: Moving beyond synthetic toys by measuring actual multi-process Google Chrome browser execution.",
        "3. Live Preemption Demonstration: Visibly showing the Convoy Effect under FCFS and its resolution via Adaptive Priority modulation.",
        "4. Academic Rigor: Truthful reporting, sub-microsecond QPC timing, and empirical validation of theoretical OS principles.",
    ], CARD_BORDER, ACCENT_CYAN)

    add_card(s10, 6.8, 1.8, 5.7, 5.0, "Artifacts & Open Source Deliverables", [
        "• Standalone Release ZIP: Portable package containing adaptive_scheduler.exe and web assets; runs on any modern 64-bit Windows laptop with double-click execution.",
        "• Public GitHub Repository: Fully open-sourced at:",
        "  https://github.com/Aryan786122/adaptive-cpu-scheduling-optimizer",
        "• Tagged Release v0.1.0 with attached precompiled portable binaries.",
        "• Comprehensive Documentation: Full theory write-up, reproducible benchmark instructions, and architectural breakdown.",
    ], CARD_BORDER, ACCENT_GREEN)

    # Save presentation
    output_path = os.path.abspath("Adaptive_CPU_Scheduling_Optimizer_Presentation.pptx")
    prs.save(output_path)
    print(f"Presentation saved successfully to: {output_path}")

if __name__ == "__main__":
    create_presentation()
