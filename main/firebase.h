#ifndef FIREBASE_H
#define FIREBASE_H

#include "esp_err.h"

#define FIREBASE_URL "https://pcb-fault-detection-9e9f0-default-rtdb.asia-southeast1.firebasedatabase.app/netlist.json"

#define JSON_BUFFER_SIZE 8192

extern char json_buffer[JSON_BUFFER_SIZE];

esp_err_t firebase_get_netlist(void);

#endif