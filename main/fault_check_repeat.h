/*
 * fault_check_repeat.h
 * ---------------------
 * Runs the resistance measurement + fault check multiple times automatically
 * and reports a CONFIDENT verdict per component instead of a single snapshot.
 *
 * Why: a single borderline reading (e.g. diode ratio 1.21 vs a 1.30 threshold)
 * can be normal measurement noise. This module repeats the measurement N times
 * and only reports a real fault if the mismatch happens consistently across
 * most/all runs -- filtering out one-off noise automatically so you don't have
 * to manually re-run and eyeball it every time.
 */

#ifndef FAULT_CHECK_REPEAT_H
#define FAULT_CHECK_REPEAT_H

#include "fault_check.h"

/* Aggregate counts from a repeated run, handed back to the caller (e.g. main.c)
 * so results can be shown on a display, sent over WiFi, logged, etc. */
typedef struct {
    int root_cause;   /* confirmed, direct-measurement faults (resistor/switch) */
    int ripple;       /* confirmed diode/led/capacitor faults explained by a root cause */
    int borderline;   /* inconsistent/ambiguous across runs -- not yet trustworthy */
    int healthy;      /* consistent pass in every run */
} fault_summary_t;

#define MAX_FAULT_DETAILS 16

/* One short, OLED-friendly summary line per non-healthy netlist entry -- built
 * for a 128x64 SSD1306 (16 chars/line at 8x8 font), so keep these terse. */
typedef struct {
    char node_pair[10];   /* e.g. "N2<->N1" */
    char type_str[10];    /* e.g. "switch" */
    char status[12];      /* e.g. "FAULT", "RIPPLE", "BORDER" */
} fault_detail_t;

/* Runs build_resistance_matrix() (from resistance.h) `num_runs` times,
 * evaluates every netlist entry against each run, and prints a final report
 * showing how many runs each entry passed/failed plus a confidence-based
 * final verdict:
 *   - FAULT CONFIRMED     : mismatched in >= fault_ratio of runs (e.g. 80%+)
 *   - BORDERLINE / NOISY   : mismatched in some but not most runs
 *   - HEALTHY              : consistent in >= fault_ratio of runs
 *
 * num_runs: how many times to repeat the measurement (recommend 5).
 * fault_ratio: fraction (0.0-1.0) of runs that must mismatch to call it a
 *              confirmed fault (recommend 0.8 = 80%).
 * raw_v: a voltage matrix (row=driven, col=read) used to cross-check any
 *        resistance-based "short" reading (a real short forces both nodes to
 *        nearly equal potential). Since building a fresh voltage matrix every
 *        repeat run would slow testing significantly, the SAME voltage
 *        snapshot (captured once, alongside your first resistance matrix) is
 *        reused for every repeat -- pass NULL to skip voltage cross-checks
 *        entirely if you don't have one available.
 * out_details: caller-provided array (size >= MAX_FAULT_DETAILS) that gets
 *              filled with one entry per non-healthy component, root-cause
 *              faults first -- pass NULL if you don't need this.
 * out_details_count: set to how many entries were written into out_details.
 *              Pass NULL if out_details is NULL.
 *
 * Returns the aggregate summary counts (also still prints the full report).
 */
fault_summary_t fault_check_run_repeated(int num_runs, float fault_ratio,
                                          const netlist_entry_t *netlist, int netlist_len,
                                          const float raw_v[NODE_COUNT][NODE_COUNT],
                                          fault_detail_t *out_details, int *out_details_count);

#endif /* FAULT_CHECK_REPEAT_H */
