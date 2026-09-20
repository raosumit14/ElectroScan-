#ifndef HARDWARE_H
#define HARDWARE_H

#include "driver/gpio.h"
#include "driver/adc.h"

/*------------- PROJECT -------------*/

#define NUM_NODES 8

#define VREF 3.3f

#define R_FIXED 10000.0f

#define STARTUP_DELAY_MS      10000     // 10 seconds

/*------------- MUX1 ----------------*/

#define MUX1_A GPIO_NUM_2
#define MUX1_B GPIO_NUM_23
#define MUX1_C GPIO_NUM_22
// #define MUX1_INH GPIO_NUM_4

/*------------- MUX2 ----------------*/

#define MUX2_A GPIO_NUM_21
#define MUX2_B GPIO_NUM_19
#define MUX2_C GPIO_NUM_18

/*------------- MUX3 ----------------*/

#define MUX3_A GPIO_NUM_26
#define MUX3_B GPIO_NUM_27
#define MUX3_C GPIO_NUM_14

/*------------- DEMUX ---------------*/

#define DEMUX_A GPIO_NUM_32
#define DEMUX_B GPIO_NUM_33
#define DEMUX_C GPIO_NUM_25
#define DEMUX_INH GPIO_NUM_15

/*------------- ADC -----------------*/

#define RES_ADC_CHANNEL ADC1_CHANNEL_6

#define VOLT_ADC_CHANNEL ADC1_CHANNEL_7

/*------------- CONTROL -------------*/

#define RES_ENABLE GPIO_NUM_5

void gpio_init(void);

void adc_init(void);

#endif