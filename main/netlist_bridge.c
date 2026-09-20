/*
 * netlist_bridge.c
 * ----------------
 * See netlist_bridge.h for overview.
 */

#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <stdio.h>
#include "netlist_bridge.h"
#include "json.h"

/* Case-insensitive match against the "type" string coming from your JSON,
 * e.g. "Resistor", "resistor", "RESISTOR" all match. Adjust the strings
 * below if your Firebase JSON uses different spelling/casing. */
static int type_matches(const char *type_str, const char *target)
{
    return strcasecmp(type_str, target) == 0;
}

int build_netlist_from_json(netlist_entry_t *out_netlist)
{
    int written = 0;

    for (int i = 0; i < connection_count; i++)
    {
        Connection *c = &connections[i];
        netlist_entry_t entry;
        memset(&entry, 0, sizeof(entry));

        entry.node_i = c->from;
        entry.node_j = c->to;

        if (type_matches(c->type, "resistor"))
        {
            entry.type = COMP_RESISTOR;
            entry.nominal_value = (float)c->expected_resistance;
        }
        else if (type_matches(c->type, "capacitor"))
        {
            entry.type = COMP_CAPACITOR;
        }
        else if (type_matches(c->type, "diode"))
        {
            entry.type = COMP_DIODE;
            /* JSON's from/to already encodes the forward (conducting)
             * direction, e.g. "N4 -> N7 (Diode)" means forward = N4->N7. */
            entry.forward_from = c->from;
            entry.forward_to = c->to;
        }
        else if (type_matches(c->type, "led"))
        {
            entry.type = COMP_LED;
            entry.forward_from = c->from;
            entry.forward_to = c->to;
        }
        else if (type_matches(c->type, "switch"))
        {
            entry.type = COMP_SWITCH;
            if (type_matches(c->state, "closed"))
                entry.expected_state = SWITCH_CLOSED;
            else
                entry.expected_state = SWITCH_OPEN;   /* default / "OPEN" */
        }
        else
        {
            printf("netlist_bridge: skipping unrecognized component type '%s' for N%d-N%d\n",
                   c->type, c->from, c->to);
            continue; /* skip entries we don't have a checker for */
        }

        out_netlist[written] = entry;
        written++;
    }

    return written;
}
