#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_rom_sys.h"

#include "hardware.h"
#include "resistance.h"


#define VREF 3.3f
#define R_FIXED 10000.0f
#define DIAG_STABILITY_THRESHOLD 1300.0f   /* healthy diagonal is normally 300-600R */
#define DIAG_RETRY_DELAY_MS      200
#define DIAG_STATUS_PRINT_EVERY  10        /* only print a status line every 10th attempt */
#define DIAG_MAX_ATTEMPTS        150       /* ~30 seconds at 200ms/attempt before giving up */

/* Call this ONCE, before starting the whole test cycle (not inside the
 * repeated-run loop). Waits until every node's diagonal reads below
 * DIAG_STABILITY_THRESHOLD, printing a status line only occasionally instead
 * of on every single attempt. Gives up after DIAG_MAX_ATTEMPTS and proceeds
 * anyway with a warning, rather than hanging forever on a truly bad channel. */
void wait_for_stable_fixture(void)
{
    int attempt = 0;
    int all_stable = 0;

    printf("\nWaiting for fixture channels to stabilize (all diagonals < %.0f ohm)...\n",
           DIAG_STABILITY_THRESHOLD);

    while (!all_stable && attempt < DIAG_MAX_ATTEMPTS)
    {
        attempt++;
        all_stable = 1;

        for (int i = 0; i < NUM_NODES; i++)
        {
            float diag = measure_resistance(i, i);

            if (diag > DIAG_STABILITY_THRESHOLD)
            {
                all_stable = 0;
                if (attempt % DIAG_STATUS_PRINT_EVERY == 1)
                {
                    printf("  Still waiting on Node %d (%.1f ohm)... attempt %d/%d\n",
                           i, diag, attempt, DIAG_MAX_ATTEMPTS);
                }
            }
        }

        if (!all_stable)
        {
            vTaskDelay(pdMS_TO_TICKS(DIAG_RETRY_DELAY_MS));
        }
    }

    if (all_stable)
    {
        printf("All fixture channels stable after %d attempt(s).\n\n", attempt);
    }
    else
    {
        printf("WARNING: Still unstable after %d attempts (~%d sec) -- proceeding anyway, "
               "but check pogo pin contact / wiring on the affected node(s).\n\n",
               attempt, (attempt * DIAG_RETRY_DELAY_MS) / 1000);
    }
}

float resistance_matrix[NUM_NODES][NUM_NODES];

/*--------------------------------------------------*/
/*                GPIO INITIALIZATION                */
/*--------------------------------------------------*/



/*--------------------------------------------------*/
/*                 ADC INITIALIZATION               */
/*--------------------------------------------------*/



/*--------------------------------------------------*/
/*                  MUX FUNCTIONS                   */
/*--------------------------------------------------*/

void set_mux1(uint8_t value)
{
    gpio_set_level(MUX1_A,value & 1);
    gpio_set_level(MUX1_B,(value>>1)&1);
    gpio_set_level(MUX1_C,(value>>2)&1);
}

void set_mux2(uint8_t value)
{
    gpio_set_level(MUX2_A,value & 1);
    gpio_set_level(MUX2_B,(value>>1)&1);
    gpio_set_level(MUX2_C,(value>>2)&1);
}

/*--------------------------------------------------*/
/*              ADC TO VOLTAGE                      */
/*--------------------------------------------------*/

float adc_to_voltage(int raw)
{
    return ((float)raw * VREF)/4095.0f;
}

/*--------------------------------------------------*/
/*           RESISTANCE MEASUREMENT                 */
/*--------------------------------------------------*/

float measure_resistance(uint8_t node1,uint8_t node2)
{
    set_mux1(node1);

    set_mux2(node2);

    esp_rom_delay_us(500);

    gpio_set_level(RES_ENABLE,1);

    esp_rom_delay_us(500);

    int sum=0;

    for(int i=0;i<10;i++)
    {
        sum+=adc1_get_raw(RES_ADC_CHANNEL);

        esp_rom_delay_us(50);
    }

    gpio_set_level(RES_ENABLE,0);

    int adc=sum/10;

    float voltage=adc_to_voltage(adc);

    if(voltage>=3.29f)
        return 1000000.0f;

    if(voltage<=0.01f)
        return 0.0f;

    float resistance=(R_FIXED*voltage)/(VREF-voltage);

    return resistance;
}

/*--------------------------------------------------*/
/*            BUILD MATRIX                          */
/*--------------------------------------------------*/

void build_resistance_matrix(void)
{
    printf("\nBuilding Resistance Matrix...\n");

    for (int row = 0; row < NUM_NODES; row++)
    {
        for (int col = 0; col < NUM_NODES; col++)
        {
            resistance_matrix[row][col] =
                measure_resistance(row, col);
        }
    }

    printf("Resistance Matrix Complete.\n");
}



/*--------------------------------------------------*/
/*              PRINT MATRIX                        */
/*--------------------------------------------------*/

void print_resistance_matrix(void)
{
    printf("\n\n=========== Resistance Matrix ===========\n\n");

    printf("        ");

    for(int j=0;j<NUM_NODES;j++)
        printf("N%-10d",j);

    printf("\n");

    for(int i=0;i<NUM_NODES;i++)
    {
        printf("N%-6d",i);

        for(int j=0;j<NUM_NODES;j++)
        {
            printf("%10.1f ",resistance_matrix[i][j]);
        }

        printf("\n");
    }
}
