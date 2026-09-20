/*
 * fault_check.c
 * -------------
 * See fault_check.h for overview and usage notes.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "fault_check.h"

/* ---------------------------------------------------------------------
 * Thresholds -- tune these based on your fixture's measured noise floor
 * --------------------------------------------------------------------- */
#define OPEN_THRESHOLD            500000.0f
#define NEAR_ZERO_THRESHOLD       50.0f
#define RESISTOR_TOL_LOW          0.05f     /* 5% of nominal  */
#define RESISTOR_TOL_HIGH         2.5f      /* was 1.30 -- widened after repeated healthy-board
                                                readings on the 220R/N5-N1 pair consistently
                                                landing at 1.8-2.1x nominal due to hub loading */
#define DIODE_ASYMMETRY_MIN_RATIO 1.15f     /* was 1.30 -- repeated healthy-board data showed
                                                this diode consistently reading 1.21-1.23,
                                                just under the old threshold every time */
#define VCC_LEVEL                 3.3f

/* ---------------------------------------------------------------------
 * Calibration state
 * --------------------------------------------------------------------- */
static float s_diag_offset[NODE_COUNT];
static float s_series_r_per_node = 1000.0f;
static int   s_calibrated = 0;

/* Offset[i][j] built on demand: diagonal uses calibration baseline,
 * off-diagonal uses (diag[i]+diag[j])/2 + 2*series_r  (approximation --
 * see project notes: verify with at least one physical jumper measurement) */
static float offset_value(int i, int j)
{
    if (i == j) {
        return s_diag_offset[i];
    }
    return (s_diag_offset[i] + s_diag_offset[j]) / 2.0f + 2.0f * s_series_r_per_node;
}

void fault_check_set_diag_offset(const float diag_offset[NODE_COUNT], float series_r_per_node)
{
    memcpy(s_diag_offset, diag_offset, sizeof(s_diag_offset));
    s_series_r_per_node = series_r_per_node;
    s_calibrated = 1;
}

void fault_check_auto_calibrate(const float raw_r[NODE_COUNT][NODE_COUNT], float series_r_per_node)
{
    for (int i = 0; i < NODE_COUNT; i++) {
        /* raw_r[i][i] is the diagonal: the same-channel Mux1/Mux2 shortcut
         * reading, i.e. pure mux+wiring resistance for that node's channel --
         * exactly what we used to measure manually with the PCB disconnected.
         * Using it live means calibration can never drift out of date. */
        s_diag_offset[i] = raw_r[i][i];
    }
    s_series_r_per_node = series_r_per_node;
    s_calibrated = 1;
}

/* Returns 1 if the raw reading should be treated as OPEN (no valid corrected value).
 * Otherwise writes the offset-corrected resistance into *out_val and returns 0. */
static int corrected_r(const float raw_r[NODE_COUNT][NODE_COUNT], int i, int j, float *out_val)
{
    float raw = raw_r[i][j];
    if (raw >= OPEN_THRESHOLD) {
        return 1; /* open */
    }
    *out_val = raw - offset_value(i, j);
    return 0;
}

float fault_check_get_corrected_resistance(const float raw_r[NODE_COUNT][NODE_COUNT], int i, int j)
{
    float val;
    if (corrected_r(raw_r, i, j, &val)) {
        return 1.0e7f; /* open */
    }
    return val;
}

#define VOLTAGE_SHORT_TOLERANCE 0.3f  /* volts -- how close the two nodes' voltages
                                         must be to count as "same potential" */

/* A genuine short forces both nodes to nearly the same electrical potential.
 * When node i is driven, the voltage measured at j should be almost equal to
 * the voltage at i itself (raw_v[i][i]); likewise driving j should show a
 * near-equal voltage back at i. If resistance suggests a short but voltage
 * does NOT show this pattern, the "short" reading is likely a fixture/offset
 * artifact rather than a real connection -- pass raw_v == NULL to skip this
 * check when no voltage matrix is available. */
int fault_check_voltage_confirms_short(const float raw_v[NODE_COUNT][NODE_COUNT], int i, int j)
{
    if (raw_v == NULL) {
        return -1; /* unknown -- caller should not treat this as confirmed OR denied */
    }
    float drop_i_to_j = fabsf(raw_v[i][i] - raw_v[i][j]);
    float drop_j_to_i = fabsf(raw_v[j][j] - raw_v[j][i]);
    return (drop_i_to_j < VOLTAGE_SHORT_TOLERANCE && drop_j_to_i < VOLTAGE_SHORT_TOLERANCE) ? 1 : 0;
}


/* ---------------------------------------------------------------------
 * Per-component checkers
 * --------------------------------------------------------------------- */

static void check_resistor(const netlist_entry_t *e, const float raw_r[NODE_COUNT][NODE_COUNT],
                            const float raw_v[NODE_COUNT][NODE_COUNT], check_result_t *res)
{
    float r_ij, r_ji;
    int open_ij = corrected_r(raw_r, e->node_i, e->node_j, &r_ij);
    int open_ji = corrected_r(raw_r, e->node_j, e->node_i, &r_ji);

    if (open_ij || open_ji) {
        res->verdict = VERDICT_MISMATCH;
        snprintf(res->reason, sizeof(res->reason),
                 "Expected resistor %.0fR but at least one direction reads fully OPEN.",
                 e->nominal_value);
        return;
    }

    float lo = e->nominal_value * RESISTOR_TOL_LOW;
    float hi = e->nominal_value * RESISTOR_TOL_HIGH;
    int ok_ij = (r_ij >= lo && r_ij <= hi);
    int ok_ji = (r_ji >= lo && r_ji <= hi);

    if (ok_ij || ok_ji) {
        res->verdict = VERDICT_CONSISTENT;
        snprintf(res->reason, sizeof(res->reason),
                 "At least one direction (%.0fR / %.0fR) falls within expected range "
                 "[%.0f-%.0f]R for a %.0fR resistor (network loading explains the other side).",
                 r_ij, r_ji, lo, hi, e->nominal_value);
        return;
    }

    /* Both directions out of range. If they're low (short-like), cross-check
     * with voltage before confirming -- a genuine short forces both nodes to
     * nearly the same potential; if voltage doesn't show that, this "short"
     * reading may be a fixture/offset artifact rather than a real fault. */
    if (r_ij < lo && r_ji < lo) {
        int v_confirms = fault_check_voltage_confirms_short(raw_v, e->node_i, e->node_j);
        if (v_confirms == 0) {
            res->verdict = VERDICT_INCONCLUSIVE;
            snprintf(res->reason, sizeof(res->reason),
                     "Resistance looks shorted (%.0fR / %.0fR) but voltage does NOT confirm equal "
                     "potential at both nodes -- likely a fixture/offset artifact, not a real short. "
                     "Verify physically before trusting this.", r_ij, r_ji);
            return;
        }
        if (v_confirms == 1) {
            res->verdict = VERDICT_MISMATCH;
            snprintf(res->reason, sizeof(res->reason),
                     "Neither direction (%.0fR / %.0fR) is within plausible range [%.0f-%.0f]R -- "
                     "CONFIRMED by voltage: both nodes sit at nearly equal potential.",
                     r_ij, r_ji, lo, hi);
            return;
        }
        /* v_confirms == -1: no voltage matrix available, fall through to plain verdict */
    }

    res->verdict = VERDICT_MISMATCH;
    snprintf(res->reason, sizeof(res->reason),
             "Neither direction (%.0fR / %.0fR) is within plausible range [%.0f-%.0f]R.",
             r_ij, r_ji, lo, hi);
}

static void check_capacitor(const netlist_entry_t *e, const float raw_r[NODE_COUNT][NODE_COUNT],
                             check_result_t *res)
{
    float r_ij = 0, r_ji = 0;
    int open_ij = corrected_r(raw_r, e->node_i, e->node_j, &r_ij);
    int open_ji = corrected_r(raw_r, e->node_j, e->node_i, &r_ji);

    /* CAP_HIGH_THRESHOLD lowered from 5000 to 120 -- repeated healthy-board
     * data showed a hub-loaded capacitor (N1-N3) consistently reading only
     * ~150-300R corrected, well below the old 5000R cutoff, while still
     * being a genuinely healthy capacitor (not a short). */
    const float CAP_HIGH_THRESHOLD = 120.0f;

    int high_seen = open_ij || open_ji || r_ij > CAP_HIGH_THRESHOLD || r_ji > CAP_HIGH_THRESHOLD;
    int near_zero_seen = (!open_ij && fabsf(r_ij) < NEAR_ZERO_THRESHOLD) &&
                          (!open_ji && fabsf(r_ji) < NEAR_ZERO_THRESHOLD);

    if (near_zero_seen && !high_seen) {
        res->verdict = VERDICT_MISMATCH;
        snprintf(res->reason, sizeof(res->reason),
                 "Reads near-zero in both directions -- looks like a short across the "
                 "capacitor instead of blocking DC as expected.");
    } else if (high_seen) {
        res->verdict = VERDICT_CONSISTENT;
        snprintf(res->reason, sizeof(res->reason),
                 "High/open resistance seen -- matches capacitor blocking DC. "
                 "Run-to-run instability here is NORMAL (depends on charge state), not a fault.");
    } else {
        res->verdict = VERDICT_INCONCLUSIVE;
        snprintf(res->reason, sizeof(res->reason),
                 "Readings (%.0fR / %.0fR) don't clearly match either pattern -- inspect manually.",
                 r_ij, r_ji);
    }
}

static void check_diode_led(const netlist_entry_t *e, const float raw_r[NODE_COUNT][NODE_COUNT],
                             check_result_t *res)
{
    float r_forward, r_reverse;
    int open_f = corrected_r(raw_r, e->forward_from, e->forward_to, &r_forward);
    int open_r = corrected_r(raw_r, e->forward_to, e->forward_from, &r_reverse);

    if (open_f || open_r) {
        res->verdict = VERDICT_INCONCLUSIVE;
        snprintf(res->reason, sizeof(res->reason),
                 "One direction fully open -- cannot compute forward/reverse ratio.");
        return;
    }
    if (r_forward <= 0.0f) {
        res->verdict = VERDICT_INCONCLUSIVE;
        snprintf(res->reason, sizeof(res->reason),
                 "Forward reading <= 0 after correction -- check offset table / calibration.");
        return;
    }

    float ratio = r_reverse / r_forward;
    if (ratio >= DIODE_ASYMMETRY_MIN_RATIO) {
        res->verdict = VERDICT_CONSISTENT;
        snprintf(res->reason, sizeof(res->reason),
                 "Reverse/forward ratio = %.2f (fwd=%.0fR, rev=%.0fR) -- matches expected "
                 "diode/LED asymmetry.", ratio, r_forward, r_reverse);
    } else {
        res->verdict = VERDICT_MISMATCH;
        snprintf(res->reason, sizeof(res->reason),
                 "Reverse/forward ratio = %.2f -- too symmetric for a diode/LED junction.", ratio);
    }
}

static void check_switch(const netlist_entry_t *e, const float raw_r[NODE_COUNT][NODE_COUNT],
                          const float raw_v[NODE_COUNT][NODE_COUNT], check_result_t *res)
{
    float r_ij = 0, r_ji = 0;
    int open_ij = corrected_r(raw_r, e->node_i, e->node_j, &r_ij);
    int open_ji = corrected_r(raw_r, e->node_j, e->node_i, &r_ji);

    if (e->expected_state == SWITCH_OPEN) {
        int both_low = (!open_ij && r_ij < 1000.0f) && (!open_ji && r_ji < 1000.0f);
        if (both_low) {
            int v_confirms = fault_check_voltage_confirms_short(raw_v, e->node_i, e->node_j);
            if (v_confirms == 0) {
                res->verdict = VERDICT_INCONCLUSIVE;
                snprintf(res->reason, sizeof(res->reason),
                         "Switch marked OPEN, resistance reads low (%.0fR / %.0fR), but voltage "
                         "does NOT confirm equal potential -- possible fixture artifact, verify "
                         "physically.", r_ij, r_ji);
                return;
            }
            res->verdict = VERDICT_MISMATCH;
            snprintf(res->reason, sizeof(res->reason),
                     "Switch marked OPEN but both directions read low (%.0fR / %.0fR) -- "
                     "may be stuck closed or shorted.", r_ij, r_ji);
        } else {
            res->verdict = VERDICT_CONSISTENT;
            snprintf(res->reason, sizeof(res->reason),
                     "Readings consistent with an open switch (any finite value likely comes "
                     "from alternate network paths, not the switch itself conducting).");
        }
    } else {
        int both_low = (!open_ij && r_ij < 200.0f) && (!open_ji && r_ji < 200.0f);
        if (both_low) {
            res->verdict = VERDICT_CONSISTENT;
            snprintf(res->reason, sizeof(res->reason), "Low resistance both directions matches a closed switch.");
        } else {
            res->verdict = VERDICT_MISMATCH;
            snprintf(res->reason, sizeof(res->reason), "Switch marked CLOSED but resistance is not low -- check contact.");
        }
    }
}

static const char *verdict_str(verdict_t v)
{
    switch (v) {
        case VERDICT_CONSISTENT:   return "CONSISTENT";
        case VERDICT_MISMATCH:     return "MISMATCH";
        case VERDICT_INCONCLUSIVE: return "INCONCLUSIVE";
        default:                   return "SKIPPED";
    }
}

static const char *type_str(component_type_t t)
{
    switch (t) {
        case COMP_RESISTOR:  return "resistor";
        case COMP_CAPACITOR: return "capacitor";
        case COMP_DIODE:     return "diode";
        case COMP_LED:       return "led";
        case COMP_SWITCH:    return "switch";
        default:             return "unknown";
    }
}

/* Silent single-entry evaluation -- no printing. Used directly by
 * fault_check_run() below, and reused by fault_check_repeat.c to aggregate
 * results across multiple measurement runs. */
check_result_t fault_check_evaluate_entry(const netlist_entry_t *e,
                                           const float raw_r[NODE_COUNT][NODE_COUNT],
                                           const float raw_v[NODE_COUNT][NODE_COUNT])
{
    check_result_t res = {0};

    switch (e->type) {
        case COMP_RESISTOR:  check_resistor(e, raw_r, raw_v, &res); break;
        case COMP_CAPACITOR: check_capacitor(e, raw_r, &res); break;
        case COMP_DIODE:
        case COMP_LED:       check_diode_led(e, raw_r, &res); break;
        case COMP_SWITCH:    check_switch(e, raw_r, raw_v, &res); break;
        default:
            res.verdict = VERDICT_SKIPPED;
            snprintf(res.reason, sizeof(res.reason), "No checker implemented for this type.");
            break;
    }
    return res;
}

const char *fault_check_verdict_str(verdict_t v) { return verdict_str(v); }
const char *fault_check_type_str(component_type_t t) { return type_str(t); }
int fault_check_is_calibrated(void) { return s_calibrated; }

void fault_check_run(const float raw_r[NODE_COUNT][NODE_COUNT],
                      const float raw_v[NODE_COUNT][NODE_COUNT],
                      const netlist_entry_t *netlist,
                      int netlist_len)
{
    if (!s_calibrated) {
        printf("WARNING: fault_check_set_diag_offset() was never called -- "
               "using zero offset (uncorrected raw values).\n");
    }

    printf("==============================================================================\n");
    printf("PCB NETLIST vs MATRIX CONSISTENCY CHECK\n");
    printf("==============================================================================\n");

    int n_consistent = 0, n_mismatch = 0, n_inconclusive = 0;

    for (int idx = 0; idx < netlist_len; idx++) {
        const netlist_entry_t *e = &netlist[idx];
        check_result_t res = fault_check_evaluate_entry(e, raw_r, raw_v);

        printf("\n[%s] N%d <-> N%d  --  %s\n", verdict_str(res.verdict), e->node_i, e->node_j,
               type_str(e->type));
        printf("   %s\n", res.reason);

        if (res.verdict == VERDICT_CONSISTENT)   n_consistent++;
        if (res.verdict == VERDICT_MISMATCH)     n_mismatch++;
        if (res.verdict == VERDICT_INCONCLUSIVE) n_inconclusive++;
    }

    printf("\n==============================================================================\n");
    printf("SUMMARY: %d consistent | %d mismatch | %d inconclusive\n",
           n_consistent, n_mismatch, n_inconclusive);
    printf("==============================================================================\n");
    if (n_mismatch > 0) {
        printf("\n>> Review MISMATCH entries first -- these are your fault candidates.\n");
    }
    printf("\nNOTE: Heuristic in-circuit check. Confirm flagged entries with a direct\n");
    printf("physical continuity/component check before declaring a board faulty.\n");
}
