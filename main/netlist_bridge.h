/*
 * netlist_bridge.h
 * ----------------
 * Converts your Firebase-downloaded, JSON-parsed `connections[]` array
 * (see json.h / json.c, struct Connection) into the netlist_entry_t array
 * that fault_check_run() expects.
 */

#ifndef NETLIST_BRIDGE_H
#define NETLIST_BRIDGE_H

#include "fault_check.h"

/* Reads the global connections[] / connection_count (from json.h) and fills
 * out_netlist with equivalent netlist_entry_t records.
 * out_netlist must have space for at least MAX_CONNECTIONS entries.
 * Returns the number of entries written (skips any with an unrecognized type). */
int build_netlist_from_json(netlist_entry_t *out_netlist);

#endif /* NETLIST_BRIDGE_H */
