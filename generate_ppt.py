import sys
import os
import pptx
from pptx.util import Inches, Pt
from pptx.enum.text import PP_ALIGN
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

    def set_slide_background(slide):
        bg = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, Inches(13.333), Inches(7.5))
        bg.fill.solid()
        bg.fill.fore_color.rgb = BG_COLOR
        bg.line.fill.background()

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
        title_box = slide.shapes.add_textbox(Inches(0.8), Inches(0.7), Inches(11.7), Inches(0.55))
        tf2 = title_box.text_frame
        tf2.word_wrap = True
        p2 = tf2.paragraphs[0]
        p2.text = title_text
        p2.font.size = Pt(21)
        p2.font.bold = True
        p2.font.color.rgb = TEXT_LIGHT

        # Subtitle
        if subtitle_text:
            sub_box = slide.shapes.add_textbox(Inches(0.8), Inches(1.2), Inches(11.7), Inches(0.4))
            tf3 = sub_box.text_frame
            tf3.word_wrap = True
            p3 = tf3.paragraphs[0]
            p3.text = subtitle_text
            p3.font.size = Pt(11)
            p3.font.color.rgb = TEXT_DIM

    def add_card(slide, left, top, width, height, title, items, border_color=CARD_BORDER, header_color=ACCENT_CYAN, font_size=11):
        card = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(left), Inches(top), Inches(width), Inches(height))
        card.fill.solid()
        card.fill.fore_color.rgb = CARD_BG
        card.line.color.rgb = border_color
        card.line.width = Pt(1)

        tb = slide.shapes.add_textbox(Inches(left + 0.2), Inches(top + 0.15), Inches(width - 0.4), Inches(height - 0.3))
        tf = tb.text_frame
        tf.word_wrap = True

        p0 = tf.paragraphs[0]
        p0.text = title
        p0.font.size = Pt(font_size + 2)
        p0.font.bold = True
        p0.font.color.rgb = header_color
        p0.space_after = Pt(8)

        for item in items:
            p = tf.add_paragraph()
            p.text = "• " + item
            p.font.size = Pt(font_size)
            p.font.color.rgb = TEXT_LIGHT
            p.space_after = Pt(4)

    def add_bordered_image(slide, img_path, left, top, width, height, border_color=CARD_BORDER):
        if not os.path.exists(img_path):
            return
        # Background border frame
        frame = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(left - 0.05), Inches(top - 0.05), Inches(width + 0.1), Inches(height + 0.1))
        frame.fill.solid()
        frame.fill.fore_color.rgb = RGBColor(15, 23, 42)
        frame.line.color.rgb = border_color
        frame.line.width = Pt(1.5)
        # Add actual picture
        slide.shapes.add_picture(img_path, Inches(left), Inches(top), width=Inches(width), height=Inches(height))

    # =========================================================================
    # SLIDE 1: TITLE SLIDE
    # =========================================================================
    s1 = prs.slides.add_slide(blank_layout)
    set_slide_background(s1)

    pill = s1.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.8), Inches(1.1), Inches(3.4), Inches(0.35))
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

    t_box = s1.shapes.add_textbox(Inches(0.8), Inches(1.6), Inches(11.7), Inches(1.8))
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

    desc_box = s1.shapes.add_textbox(Inches(0.8), Inches(3.5), Inches(11.5), Inches(1.2))
    d_tf = desc_box.text_frame
    d_tf.word_wrap = True
    dp = d_tf.paragraphs[0]
    dp.text = "An empirical demonstration comparing First-Come, First-Served (FCFS) Baseline vs. Workload-Aware Priority Modulation with Dynamic Aging Watchdogs under hardware-saturating CPU contention and multi-tab browser workloads."
    dp.font.size = Pt(13)
    dp.font.color.rgb = TEXT_DIM

    chips = [
        ("Core Technology", "Native Win32 C99 Engine (Zero Python/Node dependencies on target)", ACCENT_CYAN),
        ("Hardware Grounding", "QueryPerformanceCounter (<1µs precision) & Live Win32 Telemetry", ACCENT_GREEN),
        ("Real Application Workload", "Automated 10-Tab Google Chrome / Microsoft Edge Contention", ACCENT_AMBER),
    ]
    for idx, (head, val, col) in enumerate(chips):
        c_left = 0.8 + idx * 3.95
        card = s1.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(c_left), Inches(5.0), Inches(3.8), Inches(1.6))
        card.fill.solid()
        card.fill.fore_color.rgb = CARD_BG
        card.line.color.rgb = CARD_BORDER
        card.line.width = Pt(1)
        
        ctb = s1.shapes.add_textbox(Inches(c_left + 0.2), Inches(5.1), Inches(3.4), Inches(1.4))
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

    add_card(s2, 0.8, 1.8, 5.7, 5.1, "The Real-World Bottleneck: The Convoy Effect", [
        "Modern desktops concurrently run long batch operations (cryptographic hashing, data analytics, media rendering) alongside latency-critical interactive applications (web browsers, GUIs).",
        "Under naive First-Come, First-Served (FCFS) or equal-share round-robin scheduling, short interactive tasks are placed at the rear of ready queues.",
        "Interactive requests must wait for long compute-bound jobs to exhaust their CPU quanta (Head-of-Line Blocking).",
        "Consequence: Dropped display frames, input lag, unresponsive UI, and degraded user satisfaction despite multi-core modern hardware.",
    ], CARD_BORDER, ACCENT_AMBER)

    add_card(s2, 6.8, 1.8, 5.7, 5.1, "Research Objectives & Project Scope", [
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

    add_card(s3, 0.8, 1.8, 3.7, 5.1, "1. FCFS & Head-of-Line Blocking", [
        "Jobs are dispatched strictly in arrival sequence without interruption.",
        "Algorithm complexity is minimal: O(1) queue insertion and removal.",
        "Zero scheduling overhead and excellent hardware cache locality.",
        "Critical weakness: Severe variance in waiting time (Convoy Effect) when long burst tasks arrive prior to short interactive tasks.",
    ], CARD_BORDER, ACCENT_AMBER)

    add_card(s3, 4.8, 1.8, 3.7, 5.1, "2. Priority & Preemption", [
        "Tasks are classified into priority tiers based on burst profile and interactivity.",
        "Interactive tasks receive elevated priority; computational batch jobs are demoted.",
        "Preemptive Dispatch: The scheduler interrupts executing low-priority threads the microsecond a high-priority task arrives.",
        "Slashes interactive queue wait time by up to 90%.",
    ], CARD_BORDER, ACCENT_CYAN)

    add_card(s3, 8.8, 1.8, 3.7, 5.1, "3. Dynamic Aging & Starvation", [
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

    add_card(s4, 0.8, 4.4, 5.7, 2.5, "Kernel Interfacing & Thread Modulation", [
        "Uses Win32 SetThreadPriority to modulate worker priorities (NORMAL level 8 vs LOWEST level 4).",
        "Leak-proof process lifecycle managed via Win32 Job Objects (JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE).",
        "Hardware-aware concurrency: automatically detects and saturates all host logical processors.",
    ], CARD_BORDER, ACCENT_GREEN)

    add_card(s4, 6.8, 1.8, 5.7, 5.1, "Full-Stack Embedded Web Architecture", [
        "Zero Runtime Dependencies: Fully written in standard ISO C99; zero Python, Node.js, or external web servers required on target PC.",
        "Embedded Win32 Winsock HTTP Server: High-performance non-blocking socket loop serving static web UI and REST JSON endpoints.",
        "RESTful Control Endpoints:",
        "  • /api/telemetry — Host hardware specs & live CPU load",
        "  • /api/experiment/run_fcfs & run_adaptive — Theory lab execution",
        "  • /api/real_browser/start & status — Live browser benchmark orchestration",
        "Single-Screen Responsive Dashboard: Built with native HTML5 Canvas, modern CSS, and JavaScript event loops.",
    ], CARD_BORDER, ACCENT_CYAN)

    # =========================================================================
    # SLIDE 5: EXPERIMENT 1 — BROWSER BENCHMARK DASHBOARD (WITH SCREENSHOT)
    # =========================================================================
    s5 = prs.slides.add_slide(blank_layout)
    set_slide_background(s5)
    add_header(s5, "Real-World Benchmark", "Experiment 1: 10-Tab Browser Contention Architecture",
               "Real-time orchestration interface executing hardware-saturating background threads and Chrome tabs.")

    # Left: Explanation Cards
    add_card(s5, 0.8, 1.8, 4.8, 2.4, "1. Background CPU Contention", [
        "Spawns 24 concurrent worker threads matching host logical processors.",
        "Continuous pseudo-SHA256 & matrix arithmetic rounds without sleep.",
        "Generates genuine CPU core saturation and thread queue contention.",
    ], CARD_BORDER, ACCENT_AMBER, font_size=10)

    add_card(s5, 0.8, 4.4, 4.8, 2.5, "2. Isolated 10-Tab Chrome Launch", [
        "Launches Chrome in isolated sandbox profile (%TEMP%\\sched_browser_test_*).",
        "User personal browser profile and bookmarks remain 100% untouched.",
        "Win32 Job Object cleanly terminates test tabs upon completion.",
    ], CARD_BORDER, ACCENT_CYAN, font_size=10)

    # Right: Embedded Screenshot of the Dashboard
    add_bordered_image(s5, "screenshot_dashboard.png", 5.8, 1.8, 6.7, 5.1, ACCENT_CYAN)

    # =========================================================================
    # SLIDE 6: VISUAL COMPARISON — CHROME IN-TAB EXECUTION (WITH 2 SCREENSHOTS)
    # =========================================================================
    s6 = prs.slides.add_slide(blank_layout)
    set_slide_background(s6)
    add_header(s6, "Visual Execution Evidence", "Side-by-Side In-Tab Behavior: FCFS vs Adaptive",
               "Visual comparison of active Chrome rendering tabs under Policy A (FCFS) vs Policy B (Adaptive Priority).")

    # Left: FCFS Tab Screenshot & Card
    add_bordered_image(s6, "screenshot_tab_fcfs.png", 0.8, 1.8, 5.7, 3.4, ACCENT_AMBER)
    add_card(s6, 0.8, 5.3, 5.7, 1.8, "Policy A: FCFS Baseline (Head-of-Line Blocking)", [
        "Notice the bright Amber Banner: QUEUE CONVOY EFFECT.",
        "Progress bar creeps slowly across the screen (crawling for ~11 seconds).",
        "Status badge confirms: Queue Stalled: Waiting for Background Slices.",
    ], ACCENT_AMBER, ACCENT_AMBER, font_size=10)

    # Right: Adaptive Tab Screenshot & Card
    add_bordered_image(s6, "screenshot_tab_adaptive.png", 6.8, 1.8, 5.7, 3.4, ACCENT_GREEN)
    add_card(s6, 6.8, 5.3, 5.7, 1.8, "Policy B: Adaptive Priority (Preemptive Execution)", [
        "Notice the bright Cyan Banner: ADAPTIVE PREEMPTION: Immediate CPU Access.",
        "Progress bar rushes smoothly across the screen (finishing in ~5 seconds).",
        "Status badge confirms: Direct Preemption: Active CPU Quanta.",
    ], ACCENT_GREEN, ACCENT_GREEN, font_size=10)

    # =========================================================================
    # SLIDE 7: EMPIRICAL RESULTS — GANTT BAR COMPARISON (WITH SCREENSHOT)
    # =========================================================================
    s7 = prs.slides.add_slide(blank_layout)
    set_slide_background(s7)
    add_header(s7, "Experimental Results", "Quantitative Findings: 52.9% Faster Page Responsiveness",
               "Real millisecond measurements collected across 10 concurrent tabs on an Intel Core i7-13700HX (24 Cores).")

    # Top: Embedded Screenshot of the Gantt Bar Comparison and Results Card
    add_bordered_image(s7, "screenshot_bars_and_conclusion.png", 0.8, 1.7, 11.7, 3.3, ACCENT_GREEN)

    # Bottom: 3 Metric Analysis Cards
    add_card(s7, 0.8, 5.15, 3.7, 1.9, "1. Page Load Latency", [
        "FCFS Baseline: 11,564.3 ms",
        "Adaptive Priority: 5,448.3 ms",
        "Outcome: 52.9% Faster Page Load (✔)",
    ], CARD_BORDER, ACCENT_CYAN, font_size=10)

    add_card(s7, 4.8, 5.15, 3.7, 1.9, "2. Event Loop Lag (Jitter)", [
        "FCFS Baseline: 98.2 ms delay",
        "Adaptive Priority: 85.9 ms delay",
        "Outcome: 12.6% Lower UI Delay (✔)",
    ], CARD_BORDER, ACCENT_GREEN, font_size=10)

    add_card(s7, 8.8, 5.15, 3.7, 1.9, "3. Visual Gantt Timeline", [
        "Orange FCFS bar spans full 100% width.",
        "Blue Adaptive bar is cut in half (47%).",
        "Provides unmistakable visual proof on screen.",
    ], CARD_BORDER, ACCENT_AMBER, font_size=10)

    # =========================================================================
    # SLIDE 8: EXPERIMENT 2 — 4-STAGE THEORY LAB (WITH SCREENSHOT)
    # =========================================================================
    s8 = prs.slides.add_slide(blank_layout)
    set_slide_background(s8)
    add_header(s8, "Pedagogical Theory Lab", "Experiment 2: 4-Stage Guided OS Laboratory",
               "Connecting real Windows telemetry to calibrated workloads and interactive task injection.")

    # Left: Screenshot of Stage 1 & 2
    add_bordered_image(s8, "screenshot_theory_lab.png", 0.8, 1.8, 6.7, 5.1, ACCENT_CYAN)

    # Right: Explanation of Stages 1 & 2
    add_card(s8, 7.7, 1.8, 4.8, 2.4, "Stage 1: System Baseline Telemetry", [
        "Captures genuine host hardware telemetry via NtQuerySystemInformation.",
        "Samples live CPU utilization, physical core count, and system threads.",
        "Calculates workload scaling factors and tunes aging thresholds based on live host load.",
    ], CARD_BORDER, ACCENT_CYAN, font_size=10)

    add_card(s8, 7.7, 4.4, 4.8, 2.5, "Stage 2: Workload Mix Preparation", [
        "Prepares a controlled synthetic job queue:",
        "  • B1: Long CPU-bound matrix computation (2,400ms)",
        "  • B2: Memory-stride cache pressure worker (1,800ms)",
        "  • I1: Short interactive UI click request (35ms)",
        "Reveals the Convoy Effect under controlled lab conditions.",
    ], CARD_BORDER, ACCENT_GREEN, font_size=10)

    # =========================================================================
    # SLIDE 9: STAGE 3 & 4 — LIVE DISPATCHER & GANTT TRACES (WITH SCREENSHOT)
    # =========================================================================
    s9 = prs.slides.add_slide(blank_layout)
    set_slide_background(s9)
    add_header(s9, "Preemption in Action", "Stage 3 & 4: Live Preemption & Microsecond Replay",
               "Observing live request injection and inspecting sub-microsecond dual Gantt execution traces.")

    # Left: Explanation Cards
    add_card(s9, 0.8, 1.8, 4.8, 2.4, "Stage 3: Interactive Request Injection", [
        "User clicks 'Submit Urgent Request' while long compute job B1 is executing.",
        "FCFS: Urgent task is blocked in the ready queue until B1 completely finishes (2,400ms lag).",
        "Adaptive: B1 cooperative checkpoint yields CPU within 15ms; urgent task finishes immediately.",
    ], CARD_BORDER, ACCENT_AMBER, font_size=10)

    add_card(s9, 0.8, 4.4, 4.8, 2.5, "Stage 4: Recorded-Event Replay", [
        "Interactive playback inspector with Play, Pause, Step Back, and Step Forward controls.",
        "Inspects exact QPC arrival timestamps and dispatcher transition states.",
        "Dual microsecond Gantt tracks visibly verify task preemption and resumption slices.",
    ], CARD_BORDER, ACCENT_GREEN, font_size=10)

    # Right: Screenshot of Stage 3 & 4
    add_bordered_image(s9, "screenshot_stage34_cropped.png", 5.8, 1.8, 6.7, 5.1, ACCENT_CYAN)

    # =========================================================================
    # SLIDE 10: THE FUNDAMENTAL OS TRADE-OFF
    # =========================================================================
    s10 = prs.slides.add_slide(blank_layout)
    set_slide_background(s10)
    add_header(s10, "Theoretical Insight", "The Fundamental Operating System Trade-Off",
               "Why no single scheduling algorithm is universally superior under all operating conditions.")

    add_card(s10, 0.8, 1.8, 5.7, 5.1, "When FCFS is Optimal (Low Contention)", [
        "Zero Algorithm Overhead: FCFS incurs O(1) scheduling logic with zero kernel-mode context switches.",
        "Superior Cache Locality: Threads remain warm on their allocated physical cores without thread migration.",
        "Zero System Call Penalties: No periodic SetThreadPriority or watchdog timer interrupts.",
        "When CPU cores have idle capacity (core slack), FCFS matches or slightly beats Adaptive because priority management overhead is avoided.",
    ], CARD_BORDER, ACCENT_AMBER)

    add_card(s10, 6.8, 1.8, 5.7, 5.1, "When Adaptive is Vital (High Saturation)", [
        "Preemptive Guarantee: Prevents heavy compute jobs from holding the CPU hostage.",
        "Interactive Latency Slashed: Reduces browser load times by >50% and eliminates UI freezing.",
        "Acceptable Trade-off: Background computation experiences a minor makespan penalty in exchange for human UI responsiveness.",
        "Starvation Immunity: Dynamic aging ensures long batch tasks always achieve forward progress.",
        "Reflects the architectural philosophy of modern production schedulers (Linux CFS, Windows NT Priority Bands).",
    ], CARD_BORDER, ACCENT_GREEN)

    # =========================================================================
    # SLIDE 11: VERIFICATION & ZERO-DEPENDENCY DEPLOYMENT
    # =========================================================================
    s11 = prs.slides.add_slide(blank_layout)
    set_slide_background(s11)
    add_header(s11, "Engineering Rigor", "Verification, Testing & Zero-Dependency Portability",
               "How the system was compiled, verified, and packaged for standalone classroom demonstrations.")

    add_card(s11, 0.8, 1.8, 5.7, 5.1, "Automated Verification Pipeline", [
        "• Native C99 GCC Compilation (-O2 -Wall -std=c99): Zero errors across all subsystem modules.",
        "• Standalone Packaging Smoke Test (5/5 Stages Passed):",
        "  1. Extraction to clean temp directory: binary and assets verified.",
        "  2. Standalone launch on non-standard port (8098).",
        "  3. Win32 telemetry API live query verification.",
        "  4. Web asset delivery (HTTP 200 OK across HTML, CSS, JS).",
        "  5. Graceful termination and cleanup.",
        "• Leak-Proof Process Safety: Windows Job Objects ensure no orphaned Chrome processes remain.",
    ], CARD_BORDER, ACCENT_CYAN)

    add_card(s11, 6.8, 1.8, 5.7, 5.1, "Zero-Dependency Standalone Release", [
        "• Self-Contained Binary: adaptive_scheduler.exe statically links all required Win32 APIs (ws2_32, shell32, advapi32).",
        "• Target Machine Requirements: Zero Python, zero Node.js, and zero build toolchains required on the professor's or student's laptop.",
        "• Run Anywhere: Simply extract the release ZIP and double-click adaptive_scheduler.exe.",
        "• Automatic Browser Launch: Automatically probes available local ports (8000-8024) and launches default web browser.",
    ], CARD_BORDER, ACCENT_GREEN)

    # =========================================================================
    # SLIDE 12: CONCLUSION & REPOSITORY DELIVERABLES
    # =========================================================================
    s12 = prs.slides.add_slide(blank_layout)
    set_slide_background(s12)
    add_header(s12, "Summary & Conclusion", "Key Takeaways & Open-Source Academic Deliverables",
               "Demonstrated results, engineering accomplishments, and public open-source repository.")

    add_card(s12, 0.8, 1.8, 5.7, 5.1, "Key Project Accomplishments", [
        "1. Full-Stack Native Implementation: C99 backend + embedded HTTP server + live browser frontend with zero external dependencies.",
        "2. Real Application Validation: Moving beyond synthetic toys by measuring actual multi-process Google Chrome browser execution.",
        "3. Live Preemption Demonstration: Visibly showing the Convoy Effect under FCFS and its resolution via Adaptive Priority modulation.",
        "4. Academic Rigor: Truthful reporting, sub-microsecond QPC timing, and empirical validation of theoretical OS principles.",
    ], CARD_BORDER, ACCENT_CYAN)

    add_card(s12, 6.8, 1.8, 5.7, 5.1, "Public Deliverables & GitHub Release", [
        "• Standalone Release ZIP: Portable package containing adaptive_scheduler.exe and web assets.",
        "• Public GitHub Repository: Fully open-sourced at:",
        "  https://github.com/Aryan786122/adaptive-cpu-scheduling-optimizer",
        "• Tagged Release v0.1.0 with attached precompiled portable binaries and presentation deck.",
        "• Comprehensive Documentation: Full theory write-up, reproducible benchmark instructions, and architectural breakdown.",
    ], CARD_BORDER, ACCENT_GREEN)

    # Save presentation
    output_path = os.path.abspath("Adaptive_CPU_Scheduling_Optimizer_Presentation.pptx")
    prs.save(output_path)
    print(f"Enhanced Presentation with Screenshots saved successfully to: {output_path}")

if __name__ == "__main__":
    create_presentation()
