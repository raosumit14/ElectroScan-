/*
 * oled_display.h
 * ---------------
 * Thin wrapper around the nopnop2002/esp-idf-ssd1306 driver to show the
 * PCB fault detection summary on a 128x64 I2C SSD1306 OLED.
 *
 * Setup required before using this file:
 *   1. git clone https://github.com/nopnop2002/esp-idf-ssd1306
 *   2. Copy its components/ssd1306 folder into YOUR project's components/ folder
 *   3. Run `idf.py menuconfig` -> SSD1306 Configuration -> set Interface=I2C,
 *      SDA_GPIO, SCL_GPIO, RESET_GPIO=-1 (most 4-pin I2C boards have no reset pin)
 */

#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "fault_check_repeat.h"

/* Call once at startup, after gpio_init()/adc_init(), before running tests. */
void oled_display_init(void);

/* Shows a simple startup / status message, e.g. "Booting..." or "WiFi OK". */
void oled_display_status(const char *line1, const char *line2);

/* Shows the aggregated fault_summary_t from fault_check_run_repeated() as a
 * clean, presentation-friendly screen: pass/fail counts + top-line verdict. */
void oled_display_summary(fault_summary_t summary);

/* Cycles through each detail line (from fault_check_run_repeated's out_details),
 * showing one per screen for `delay_ms` milliseconds each. Call this AFTER
 * oled_display_summary() so the audience sees the overall verdict first,
 * then the specific faulty component(s). Safe to call with count == 0 (no-op). */
void oled_display_fault_details(const fault_detail_t *details, int count, int delay_ms);

#endif /* OLED_DISPLAY_H */
