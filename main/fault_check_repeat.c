/*
 * fault_check_repeat.c
 * ---------------------
 * See fault_check_repeat.h for overview.
 */

#include <stdio.h>
#include <math.h>
#include "fault_check_repeat.h"
#include "resistance.h"   /* build_resistance_matrix(), resistance_matrix[][] */

#define MAX_NETLIST_FOR_REPEAT 32   /* must be >= your real netlist size (MAX_CONNECTIONS) */

fault_summary_t fault_check_run_repeated(int num_runs, float fault_ratio,
                                          const netlist_entry_t *netlist, int netlist_len,
                                          const float raw_v[NODE_COUNT][NODE_COUNT],
                                          fault_detail_t *out_details, int *out_details_count)
{
    fault_summary_t summary = {0, 0, 0, 0};
    if (out_details_count != NULL) *out_details_count = 0;

    if (netlist_len > MAX_NETLIST_FOR_REPEAT) {
        printf("fault_check_run_repeated: netlist_len (%d) exceeds MAX_NETLIST_FOR_REPEAT (%d), "
               "increase the macro.\n", netlist_len, MAX_NETLIST_FOR_REPEAT);
        return summary;
    }

    static int consistent_count[MAX_NETLIST_FOR_REPEAT];
    static int mismatch_count[MAX_NETLIST_FOR_REPEAT];
    static int inconclusive_count[MAX_NETLIST_FOR_REPEAT];
    static char last_mismatch_reason[MAX_NETLIST_FOR_REPEAT][256];

    for (int i = 0; i < MAX_NETLIST_FOR_REPEAT; i++) {
        consistent_count[i] = 0;
        mismatch_count[i] = 0;
        inconclusive_count[i] = 0;
        last_mismatch_reason[i][0] = '\0';
    }

    /* Coverage map: mark every node pair that the netlist actually expects a
     * component between. Any pair NOT covered is checked separately below --
     * not for being "open" (in-circuit indirect paths make almost every pair
     * read some finite resistance even when healthy, so that test would be
     * constantly noisy), but for reading a genuine near-zero SHORT, which no
     * ordinary indirect network path produces on this board. */
    static int covered[NODE_COUNT][NODE_COUNT];
    for (int i = 0; i < NODE_COUNT; i++)
        for (int j = 0; j < NODE_COUNT; j++)
            covered[i][j] = (i == j) ? 1 : 0;
    for (int idx = 0; idx < netlist_len; idx++) {
        covered[netlist[idx].node_i][netlist[idx].node_j] = 1;
        covered[netlist[idx].node_j][netlist[idx].node_i] = 1;
    }

    #define UNEXPECTED_SHORT_THRESHOLD 100.0f  /* both directions below this = flag it */
    static int unexpected_mismatch_count[NODE_COUNT][NODE_COUNT];
    for (int i = 0; i < NODE_COUNT; i++)
        for (int j = 0; j < NODE_COUNT; j++)
            unexpected_mismatch_count[i][j] = 0;

    printf("==============================================================================\n");
    printf("REPEATED FAULT CHECK -- %d runs, confirm-fault threshold = %.0f%% mismatch\n",
           num_runs, fault_ratio * 100.0f);
    printf("==============================================================================\n");

    for (int run = 0; run < num_runs; run++) {
        printf("\n--- Run %d/%d ---\n", run + 1, num_runs);

        build_resistance_matrix();  /* refreshes the global resistance_matrix[][] */
        fault_check_auto_calibrate(resistance_matrix, 1000.0f);  /* re-calibrate from THIS run's own diagonal -- never goes stale */

        for (int idx = 0; idx < netlist_len; idx++) {
            const netlist_entry_t *e = &netlist[idx];
            check_result_t res = fault_check_evaluate_entry(e, resistance_matrix, raw_v);

            if (res.verdict == VERDICT_CONSISTENT)   consistent_count[idx]++;
            if (res.verdict == VERDICT_MISMATCH) {
                mismatch_count[idx]++;
                snprintf(last_mismatch_reason[idx], sizeof(last_mismatch_reason[idx]), "%s", res.reason);
            }
            if (res.verdict == VERDICT_INCONCLUSIVE) inconclusive_count[idx]++;

            printf("  N%d<->N%d (%s): %s\n", e->node_i, e->node_j,
                   fault_check_type_str(e->type), fault_check_verdict_str(res.verdict));
        }

        /* Scan every uncovered pair for an unexpected short (near-zero both
         * directions) -- something the netlist doesn't claim should exist. */
        for (int i = 0; i < NODE_COUNT; i++) {
            for (int j = i + 1; j < NODE_COUNT; j++) {
                if (covered[i][j]) continue;
                float r_ij = fault_check_get_corrected_resistance(resistance_matrix, i, j);
                float r_ji = fault_check_get_corrected_resistance(resistance_matrix, j, i);
                if (fabsf(r_ij) < UNEXPECTED_SHORT_THRESHOLD && fabsf(r_ji) < UNEXPECTED_SHORT_THRESHOLD) {
                    unexpected_mismatch_count[i][j]++;
                }
            }
        }
    }

    printf("\n==============================================================================\n");
    printf("FINAL REPORT (aggregated over %d runs)\n", num_runs);
    printf("==============================================================================\n");

    /* Phase 1: compute each entry's final verdict (HEALTHY / BORDERLINE / CONFIRMED)
     * without printing yet, so we can do root-cause grouping before showing results. */
    typedef enum { FV_HEALTHY, FV_BORDERLINE, FV_CONFIRMED } final_verdict_t;
    static final_verdict_t final_verdict[MAX_NETLIST_FOR_REPEAT];
    static int is_ripple_of[MAX_NETLIST_FOR_REPEAT]; /* index of root-cause entry, or -1 */

    for (int idx = 0; idx < netlist_len; idx++) {
        float mismatch_frac = (float)mismatch_count[idx] / (float)num_runs;
        is_ripple_of[idx] = -1;

        if (mismatch_frac >= fault_ratio) {
            final_verdict[idx] = FV_CONFIRMED;
        } else if (mismatch_count[idx] > 0 || consistent_count[idx] < num_runs) {
            final_verdict[idx] = FV_BORDERLINE;
        } else {
            final_verdict[idx] = FV_HEALTHY;
        }
    }

    /* Phase 2: root-cause grouping. A component whose check directly measures
     * an absolute value against a fixed expectation (resistor: nominal ohms,
     * switch: open/closed state) is a strong, "objective" fault signal. A
     * component whose check depends on a ratio or blocking-behavior pattern
     * (diode, led, capacitor) is more sensitive to network loading from
     * elsewhere on the board -- and we've confirmed empirically that this
     * ripple effect propagates through the shared hub node even when the
     * two components don't directly touch the same node pair (e.g. a switch
     * at N2-N1 shifted a diode reading at N4-N7, several hops away, purely
     * through how current redistributes across the shared N1 hub). Because
     * of this multi-hop propagation, we don't require a direct shared node --
     * if ANY resistor/switch fault is confirmed OR borderline, every
     * diode/led/capacitor fault confirmed in the SAME run set is treated as
     * a likely ripple effect of that root cause, not an independent fault.
     * BORDERLINE is included deliberately: a weak/intermittent short can be
     * too marginal to consistently cross the resistor's own (wide) tolerance
     * band, yet still consistently disturb a diode/LED's much narrower
     * ratio check on every single run -- observed directly on real hardware,
     * where a loose jumper produced a 60% resistor mismatch (borderline) but
     * a 100% diode/LED mismatch (would otherwise misreport as two separate
     * confirmed faults instead of one likely root cause). */
    int first_root_cause_idx = -1;
    int first_root_cause_is_confirmed = 0;
    for (int idx = 0; idx < netlist_len; idx++) {
        component_type_t t = netlist[idx].type;
        if (t != COMP_RESISTOR && t != COMP_SWITCH) continue;
        if (final_verdict[idx] == FV_CONFIRMED) {
            first_root_cause_idx = idx;
            first_root_cause_is_confirmed = 1;
            break;
        }
        if (final_verdict[idx] == FV_BORDERLINE && mismatch_count[idx] > 0 && first_root_cause_idx < 0) {
            first_root_cause_idx = idx; /* keep looking in case a fully CONFIRMED one shows up later */
        }
    }

    if (first_root_cause_idx >= 0) {
        for (int idx = 0; idx < netlist_len; idx++) {
            if (idx == first_root_cause_idx) continue;
            if (final_verdict[idx] != FV_CONFIRMED) continue;
            component_type_t t = netlist[idx].type;
            if (t == COMP_DIODE || t == COMP_LED || t == COMP_CAPACITOR) {
                is_ripple_of[idx] = first_root_cause_idx;
            }
        }
    }
    (void)first_root_cause_is_confirmed; /* available if callers want to distinguish confidence later */

    /* Phase 2b: resistor/switch vs resistor/switch clustering. Unlike the
     * diode/LED/capacitor case above, we can't assume one resistor is more
     * "trustworthy" than another -- both are direct measurements. But a real
     * physical short at one node will drag down EVERY resistor connected to
     * that same node (we confirmed this: shorting N0-N3 also made N2-N0 read
     * collapsed, purely because N2 sits one resistor-hop from N0). So instead
     * we group by SHARED NODE (not just "any root cause"), and within a
     * cluster, treat whichever reading is closest to 0 ohms as the likely
     * real short -- the others are probably just electrically dragged down
     * by being wired to the same node, not independently broken. This is
     * still a heuristic, not proof -- always confirm physically. */
    static int is_cluster_member_of[MAX_NETLIST_FOR_REPEAT]; /* -1, or index of cluster's primary */
    for (int i = 0; i < MAX_NETLIST_FOR_REPEAT; i++) is_cluster_member_of[i] = -1;

    for (int idx = 0; idx < netlist_len; idx++) {
        if (final_verdict[idx] != FV_CONFIRMED) continue;
        component_type_t t = netlist[idx].type;
        if (t != COMP_RESISTOR && t != COMP_SWITCH) continue;
        if (is_cluster_member_of[idx] != -1) continue; /* already grouped */

        /* Find every other confirmed resistor/switch fault sharing a node with idx */
        int cluster[MAX_NETLIST_FOR_REPEAT];
        int cluster_size = 0;
        cluster[cluster_size++] = idx;

        for (int other = 0; other < netlist_len; other++) {
            if (other == idx || final_verdict[other] != FV_CONFIRMED) continue;
            component_type_t ot = netlist[other].type;
            if (ot != COMP_RESISTOR && ot != COMP_SWITCH) continue;

            int shares_node = (netlist[idx].node_i == netlist[other].node_i ||
                               netlist[idx].node_i == netlist[other].node_j ||
                               netlist[idx].node_j == netlist[other].node_i ||
                               netlist[idx].node_j == netlist[other].node_j);
            if (shares_node) cluster[cluster_size++] = other;
        }

        if (cluster_size <= 1) continue; /* no sharing -- stands alone */

        /* Pick the cluster member whose reading is closest to 0 ohms (most
         * collapsed) as the likely real short. */
        int primary = cluster[0];
        float best_severity = 1.0e9f;
        for (int c = 0; c < cluster_size; c++) {
            int m = cluster[c];
            float r_ij = fault_check_get_corrected_resistance(resistance_matrix, netlist[m].node_i, netlist[m].node_j);
            float r_ji = fault_check_get_corrected_resistance(resistance_matrix, netlist[m].node_j, netlist[m].node_i);
            float severity = fminf(fabsf(r_ij), fabsf(r_ji)); /* smaller = more collapsed = more severe */
            if (severity < best_severity) {
                best_severity = severity;
                primary = m;
            }
        }
        for (int c = 0; c < cluster_size; c++) {
            int m = cluster[c];
            is_cluster_member_of[m] = (m == primary) ? -1 : primary;
        }
    }

    /* Phase 3: print, using the grouping computed above. Also collect a
     * short detail line per non-healthy entry for the caller (e.g. OLED),
     * root-cause faults first so the most important thing shows up first. */
    int n_confirmed_fault = 0, n_borderline = 0, n_healthy = 0, n_ripple = 0;
    int details_written = 0;

    /* Pass A: root-cause CONFIRMED faults (highest priority for display) */
    for (int idx = 0; idx < netlist_len; idx++) {
        if (final_verdict[idx] == FV_CONFIRMED && is_ripple_of[idx] < 0 && is_cluster_member_of[idx] < 0 &&
            out_details != NULL && details_written < MAX_FAULT_DETAILS) {
            const netlist_entry_t *e = &netlist[idx];
            snprintf(out_details[details_written].node_pair, sizeof(out_details[0].node_pair),
                     "N%d<->N%d", e->node_i, e->node_j);
            snprintf(out_details[details_written].type_str, sizeof(out_details[0].type_str),
                     "%s", fault_check_type_str(e->type));
            snprintf(out_details[details_written].status, sizeof(out_details[0].status), "FAULT");
            details_written++;
        }
    }
    /* Pass B: ripple effects, cluster members, and borderline entries (lower priority) */
    for (int idx = 0; idx < netlist_len; idx++) {
        if (final_verdict[idx] == FV_HEALTHY) continue;
        if (final_verdict[idx] == FV_CONFIRMED && is_ripple_of[idx] < 0 && is_cluster_member_of[idx] < 0) continue; /* already added in Pass A */
        if (out_details == NULL || details_written >= MAX_FAULT_DETAILS) break;

        const netlist_entry_t *e = &netlist[idx];
        snprintf(out_details[details_written].node_pair, sizeof(out_details[0].node_pair),
                 "N%d<->N%d", e->node_i, e->node_j);
        snprintf(out_details[details_written].type_str, sizeof(out_details[0].type_str),
                 "%s", fault_check_type_str(e->type));
        snprintf(out_details[details_written].status, sizeof(out_details[0].status),
                 (final_verdict[idx] == FV_CONFIRMED) ? "RIPPLE" : "BORDER");
        details_written++;
    }
    if (out_details_count != NULL) *out_details_count = details_written;

    for (int idx = 0; idx < netlist_len; idx++) {
        const netlist_entry_t *e = &netlist[idx];

        printf("\nN%d <-> N%d (%s):\n", e->node_i, e->node_j, fault_check_type_str(e->type));
        printf("  consistent=%d  mismatch=%d  inconclusive=%d  (of %d runs)\n",
               consistent_count[idx], mismatch_count[idx], inconclusive_count[idx], num_runs);

        if (final_verdict[idx] == FV_CONFIRMED && is_ripple_of[idx] >= 0) {
            int root = is_ripple_of[idx];
            printf("  >> LIKELY RIPPLE EFFECT (probably not a separate fault)\n");
            printf("     N%d<->N%d (%s) also %s -- that's a direct measurement, more trustworthy.\n",
                   netlist[root].node_i, netlist[root].node_j, fault_check_type_str(netlist[root].type),
                   final_verdict[root] == FV_CONFIRMED ? "failed" : "showed a borderline/inconsistent reading");
            printf("     This component's check is ratio-based, so it can get thrown off by faults\n");
            printf("     elsewhere on the board. Fix/re-check the root cause first, then re-check this one.\n");
            n_ripple++;
        } else if (final_verdict[idx] == FV_CONFIRMED && is_cluster_member_of[idx] >= 0) {
            int root = is_cluster_member_of[idx];
            printf("  >> SAME FAULT CLUSTER (likely not a separate fault)\n");
            printf("     Shares a node with N%d<->N%d (%s), which reads more severely collapsed --\n",
                   netlist[root].node_i, netlist[root].node_j, fault_check_type_str(netlist[root].type));
            printf("     that one is more likely the actual short. This reading may just be dragged\n");
            printf("     down electrically by sitting on the same node. Confirm physically either way.\n");
            n_ripple++;
        } else if (final_verdict[idx] == FV_CONFIRMED) {
            float mismatch_frac = (float)mismatch_count[idx] / (float)num_runs;
            printf("  >> FAULT CONFIRMED (mismatched in %.0f%% of runs)\n", mismatch_frac * 100.0f);
            printf("     Last mismatch reason: %s\n", last_mismatch_reason[idx]);
            n_confirmed_fault++;
        } else if (final_verdict[idx] == FV_BORDERLINE && mismatch_count[idx] > 0) {
            float mismatch_frac = (float)mismatch_count[idx] / (float)num_runs;
            printf("  >> BORDERLINE / NOISY -- mismatched in %.0f%% of runs, not consistent enough "
                   "to call a confirmed fault. Likely measurement noise near a threshold.\n",
                   mismatch_frac * 100.0f);
            printf("     Example mismatch reason seen: %s\n", last_mismatch_reason[idx]);
            n_borderline++;
        } else if (final_verdict[idx] == FV_HEALTHY) {
            printf("  >> HEALTHY (consistent in all %d runs)\n", num_runs);
            n_healthy++;
        } else {
            printf("  >> INCONCLUSIVE -- no outright mismatch, but only consistent in %d/%d runs "
                   "(rest were inconclusive/ambiguous readings). Not yet a confirmed fault, but "
                   "not a clean pass either -- worth a closer look or a threshold tweak for this "
                   "component.\n", consistent_count[idx], num_runs);
            n_borderline++;
        }
    }

    /* Report any uncovered pair that consistently read as an unexpected short */
    int n_unexpected = 0;
    printf("\n------------------------------------------------------------------------------\n");
    printf("UNEXPECTED CONNECTIONS (pairs NOT in netlist, checked for accidental shorts)\n");
    printf("------------------------------------------------------------------------------\n");
    for (int i = 0; i < NODE_COUNT; i++) {
        for (int j = i + 1; j < NODE_COUNT; j++) {
            if (covered[i][j]) continue;
            float frac = (float)unexpected_mismatch_count[i][j] / (float)num_runs;
            if (frac >= fault_ratio) {
                printf("  N%d<->N%d: UNEXPECTED SHORT (near-zero in %.0f%% of runs) -- not in netlist,\n",
                       i, j, frac * 100.0f);
                printf("     check for a solder bridge or accidental jumper between these two nodes.\n");
                n_unexpected++;
            }
        }
    }
    if (n_unexpected == 0) {
        printf("  None found -- no uncovered pair reads as an unexpected short.\n");
    }

    printf("\n==============================================================================\n");
    printf("OVERALL: %d root-cause fault | %d likely ripple effect | %d borderline/noisy | %d healthy | %d unexpected connection\n",
           n_confirmed_fault, n_ripple, n_borderline, n_healthy, n_unexpected);
    printf("==============================================================================\n");
    if (n_confirmed_fault > 0) {
        printf("\n>> Physically inspect the ROOT-CAUSE FAULT entries above first -- fixing these\n");
        printf("   will likely resolve the RIPPLE EFFECT entries too, without touching them directly.\n");
    } else if (n_ripple > 0) {
        printf("\n>> All confirmed mismatches were classified as ripple effects with no clear direct\n");
        printf("   root cause found in this netlist -- inspect these manually, the true cause may be\n");
        printf("   a component not covered by your current netlist.\n");
    }
    if (n_borderline > 0) {
        printf(">> BORDERLINE entries are not yet trustworthy as faults -- consider more runs,\n");
        printf("   or slightly loosen that component's threshold in fault_check.c if this\n");
        printf("   component consistently sits near the line on a known-healthy board.\n");
    }

    summary.root_cause = n_confirmed_fault;
    summary.ripple      = n_ripple;
    summary.borderline  = n_borderline;
    summary.healthy     = n_healthy;
    return summary;
}
