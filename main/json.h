#ifndef JSON_H
#define JSON_H

#include "hardware.h"

#define MAX_CONNECTIONS 32

typedef struct
{
    int from;                      // 0-7
    int to;                        // 0-7

    char type[20];

    int expected_resistance;       // Only for resistors

    char state[10];                // Only for switches

} Connection;

extern Connection connections[MAX_CONNECTIONS];

extern int connection_count;

void parse_json(const char *json_string);

void print_connections(void);

#endif