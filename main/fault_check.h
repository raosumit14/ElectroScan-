/*
 * fault_check.h
 * -------------
 * PCB Fault Detection - Netlist vs Matrix Comparator (C / ESP-IDF version)
 *
 * Given a measured Resistance matrix, Voltage matrix, a diagonal calibration
 * table, and a netlist (expected components between node pairs), this module
 * checks whether measured electrical behavior is CONSISTENT, MISMATCH, or
 * INCONCLUSIVE for each netlist entry.
 *
 * LIMITATION: This is an in-circuit heuristic check, not absolute proof.
 * Every reading includes parallel/alternate paths through the rest of the
 * network, not just the single listed component. Use this to triage/flag
 * candidates for physical inspection, not as a final verdict.
 */

#ifndef FAULT_CHECK_H
#define FAULT_CHECK_H

#define NODE_COUNT 8

typedef enum {
    COMP_RESISTOR,
    COMP_CAPACITOR,
    COMP_DIODE,
    COMP_LED,
    COMP_SWITCH
} component_type_t;

typedef enum {
    SWITCH_OPEN,
    SWITCH_CLOSED
} switch_state_t;

typedef enum {
    VERDICT_CONSISTENT,
    VERDICT_MISMATCH,
    VERDICT_INCONCLUSIVE,
    VERDICT_SKIPPED
} verdict_t;

typedef struct {
    int node_i;                 /* index 0..NODE_COUNT-1, e.g. N0 = 0 */
    int node_j;
    component_type_t type;
    float nominal_value;        /* resistor ohms; ignored for other types */
    int forward_from;           /* for diode/led: node index of anode/conducting side */
    int forward_to;             /* for diode/led: node index of cathode/receiving side */
    switch_state_t expected_state; /* for switch entries */
} netlist_entry_t;

typedef struct {
    verdict_t verdict;
    char reason[256];
} check_result_t;

/* Call once at startup with your fixture's diagonal calibration values (Ohms).
 * Manual/one-time version -- prefer fault_check_auto_calibrate() below instead,
 * since it re-reads fresh values every run and never goes stale. */
void fault_check_set_diag_offset(const float diag_offset[NODE_COUNT], float series_r_per_node);

/* Self-calibrating version: reads the diagonal (self-resistance) directly out
 * of THIS run's own raw_r matrix -- raw_r[i][i] -- and uses that as the offset
 * for node i, instead of a hardcoded value from a previous session. Call this
 * every time right after build_resistance_matrix(), before evaluating any
 * netlist entries, so calibration can never go stale/drift out of date.
 * series_r_per_node: the fixed series resistor value per node (e.g. 1000.0f
 * for a 1k resistor) used to build the off-diagonal offset estimate. */
void fault_check_auto_calibrate(const float raw_r[NODE_COUNT][NODE_COUNT], float series_r_per_node);

/* Run the full netlist check against a raw R matrix + raw V matrix.
 * raw_r[i][j] and raw_v[i][j]: row = node driven, col = node read.
 * Use 1000000.0f (or higher) to represent a saturated/open reading.
 * netlist / netlist_len: your board's expected component list.
 * Prints a full report via printf (redirect to ESP_LOGI if preferred). */
void fault_check_run(const float raw_r[NODE_COUNT][NODE_COUNT],
                      const float raw_v[NODE_COUNT][NODE_COUNT],
                      const netlist_entry_t *netlist,
                      int netlist_len);

/* Silent, single-entry check (no printing) -- used internally by fault_check_run()
 * and reusable by other modules (e.g. a repeat-run aggregator) that need the
 * raw verdict for one netlist entry against one R matrix snapshot.
 * raw_v may be NULL if no voltage matrix is available -- voltage-based
 * cross-checks (short confirmation) are simply skipped in that case. */
check_result_t fault_check_evaluate_entry(const netlist_entry_t *e,
                                           const float raw_r[NODE_COUNT][NODE_COUNT],
                                           const float raw_v[NODE_COUNT][NODE_COUNT]);

const char *fault_check_verdict_str(verdict_t v);
const char *fault_check_type_str(component_type_t t);
int fault_check_is_calibrated(void);

/* Returns the offset-corrected resistance for pair (i,j), or a large sentinel
 * value (1.0e7f) if that direction reads fully open. Used by the repeat-run
 * aggregator to compare severity between two confirmed resistor/switch faults
 * that share a node, so it can pick which one is most likely the real short. */
float fault_check_get_corrected_resistance(const float raw_r[NODE_COUNT][NODE_COUNT], int i, int j);

/* Returns 1 if the voltage matrix confirms nodes i and j sit at nearly equal
 * potential (consistent with a genuine short), 0 if it does NOT (the "short"
 * reading is likely a fixture/offset artifact), or -1 if raw_v is NULL
 * (voltage unavailable -- caller should not treat this as confirmed or denied). */
int fault_check_voltage_confirms_short(const float raw_v[NODE_COUNT][NODE_COUNT], int i, int j);

#endif /* FAULT_CHECK_H */
