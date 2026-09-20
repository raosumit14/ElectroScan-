
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fault_check_repeat.h"
#include "hardware.h"
#include "resistance.h"
#include "voltage.h"
#include "firebase.h"
#include "wifi.h"
#include "json.h"
#include "fault_check.h"
#include "netlist_bridge.h"
#include "oled_display.h"   
void app_main(void)
{
    printf("\n=========================================\n");
    printf("     PCB FAULT DETECTION SYSTEM\n");
    printf("=========================================\n");

    /* Initialize Hardware */
    gpio_init();
    adc_init();

    // oled display init and status
    oled_display_init();                             
    oled_display_status("PCB FAULT SYSTEM", "Starting...");

    /* Connect to WiFi */
    wifi_init_sta();

    printf("\nWiFi Connected Successfully!\n");
    oled_display_status("WiFi Connected", "Getting netlist...");

    /* Download JSON from Firebase */
    printf("\nDownloading Netlist...\n");

    if (firebase_get_netlist() == ESP_OK)
    {
        printf("\nNetlist Download Successful\n");
        parse_json(json_buffer);

        print_connections();
    }
    else
    {
        printf("\nFailed to Download Netlist\n");
    }

    wait_for_stable_fixture();   /* once, before anything else measures */


    /* Generate Resistance Matrix */
    printf("\nGenerating Resistance Matrix...\n");

    build_resistance_matrix();

    print_resistance_matrix();

    /* Generate Voltage Matrix */
    printf("\nGenerating Voltage Matrix...\n");

    build_voltage_matrix();

    print_voltage_matrix();

     
     /* --- Fault Detection --- */                      
    printf("\nRunning Fault Detection...\n");

   /*float diag_offset[NUM_NODES] = {
        370.7f, 366.1f, 370.2f, 430.5f, 363.4f, 369.2f, 362.4f, 367.1f
    };
    fault_check_set_diag_offset(diag_offset, 1000.0f);*/

    /*netlist_entry_t netlist[MAX_CONNECTIONS];
    int netlist_len = build_netlist_from_json(netlist);

    fault_summary_t summary = fault_check_run_repeated(5, 0.8f, netlist, netlist_len);
    oled_display_summary(summary);  */
                       /* <-- shows result on OLED */

                       static netlist_entry_t netlist[MAX_CONNECTIONS];
    int netlist_len = build_netlist_from_json(netlist);

    static fault_detail_t details[MAX_FAULT_DETAILS];
    int details_count = 0;
    fault_summary_t summary = fault_check_run_repeated(5, 0.8f, netlist, netlist_len,
                                                       voltage_matrix, details, &details_count);

    oled_display_summary(summary);
    vTaskDelay(pdMS_TO_TICKS(3000));
    oled_display_fault_details(details, details_count, 2500);
    /* --- End Fault Detection --- */

    printf("\n=========================================\n");
    printf(" Matrix Generation Completed\n");
    printf("=========================================\n");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}