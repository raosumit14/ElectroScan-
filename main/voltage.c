#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_rom_sys.h"

#include "hardware.h"
#include "voltage.h"

#define VREF 3.3f

float voltage_matrix[NUM_NODES][NUM_NODES];

/*------------------------------------------------*/
/*              MUX3 SELECT                       */
/*------------------------------------------------*/

void set_mux3(uint8_t value)
{
    gpio_set_level(MUX3_A, value & 1);
    gpio_set_level(MUX3_B, (value >> 1) & 1);
    gpio_set_level(MUX3_C, (value >> 2) & 1);
}

/*------------------------------------------------*/
/*             DEMUX SELECT                       */
/*------------------------------------------------*/

void set_demux(uint8_t value)
{
    gpio_set_level(DEMUX_A, value & 1);
    gpio_set_level(DEMUX_B, (value >> 1) & 1);
    gpio_set_level(DEMUX_C, (value >> 2) & 1);
}

/*------------------------------------------------*/
/*          ADC RAW TO VOLTAGE                    */
/*------------------------------------------------*/

float adc_to_voltage_voltage(int raw)
{
    return ((float)raw * VREF) / 4095.0f;
}

/*------------------------------------------------*/
/*          SINGLE VOLTAGE MEASUREMENT            */
/*------------------------------------------------*/

float measure_voltage(uint8_t inject_node,
                      uint8_t read_node)
{
    set_demux(inject_node);

    set_mux3(read_node);

    gpio_set_level(DEMUX_INH, 0);

    esp_rom_delay_us(500);

    int sum = 0;

    for(int i = 0; i < 10; i++)
    {
        sum += adc1_get_raw(VOLT_ADC_CHANNEL);

        esp_rom_delay_us(50);
    }

    gpio_set_level(DEMUX_INH, 1);

    int adc = sum / 10;

    return adc_to_voltage_voltage(adc);
}

/*------------------------------------------------*/
/*         BUILD VOLTAGE MATRIX                   */
/*------------------------------------------------*/

void build_voltage_matrix(void)
{
    printf("\nBuilding Voltage Matrix...\n\n");

    for(int inject = 0; inject < NUM_NODES; inject++)
    {
        for(int read = 0; read < NUM_NODES; read++)
        {
            voltage_matrix[inject][read] =
                measure_voltage(inject, read);

            
        }
    }
}

/*------------------------------------------------*/
/*          PRINT MATRIX                          */
/*------------------------------------------------*/

void print_voltage_matrix(void)
{
    printf("\n\n=========== Voltage Matrix ===========\n\n");

    printf("        ");

    for(int j = 0; j < NUM_NODES; j++)
        printf("N%-8d", j);

    printf("\n");

    for(int i = 0; i < NUM_NODES; i++)
    {
        printf("N%-6d", i);

        for(int j = 0; j < NUM_NODES; j++)
        {
            printf("%8.3f ",
                   voltage_matrix[i][j]);
        }

        printf("\n");
    }
}