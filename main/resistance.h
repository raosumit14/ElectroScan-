#ifndef RESISTANCE_H
#define RESISTANCE_H

#include "hardware.h"

extern float resistance_matrix[NUM_NODES][NUM_NODES];

void gpio_init(void);
void adc_init(void);
void wait_for_stable_fixture(void);

void set_mux1(uint8_t channel);
void set_mux2(uint8_t channel);

float adc_to_voltage(int adc_raw);
float measure_resistance(uint8_t node1, uint8_t node2);

void build_resistance_matrix(void);
void print_resistance_matrix(void);

#endif