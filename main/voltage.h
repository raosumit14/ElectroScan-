#ifndef VOLTAGE_H
#define VOLTAGE_H

#include "hardware.h"

extern float voltage_matrix[NUM_NODES][NUM_NODES];

void set_mux3(uint8_t channel);
void set_demux(uint8_t channel);

float adc_to_voltage_voltage(int adc_raw);

float measure_voltage(uint8_t inject_node,
                      uint8_t read_node);

void build_voltage_matrix(void);

void print_voltage_matrix(void);

#endif