/*
 * oled_display.c
 * ---------------
 * See oled_display.h for setup instructions.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oled_display.h"
#include "ssd1306.h"

/* TODO: confirm these match what you set in `idf.py menuconfig` ->
 * SSD1306 Configuration. CONFIG_SDA_GPIO / CONFIG_SCL_GPIO / CONFIG_RESET_GPIO
 * come from that menuconfig step automatically once configured -- these
 * #ifndef guards just give safe fallbacks if you haven't run menuconfig yet. */
#ifndef CONFIG_SDA_GPIO
#define CONFIG_SDA_GPIO 21
#endif
#ifndef CONFIG_SCL_GPIO
#define CONFIG_SCL_GPIO 22
#endif
#ifndef CONFIG_RESET_GPIO
#define CONFIG_RESET_GPIO -1
#endif

static SSD1306_t s_dev;

void oled_display_init(void)
{
    i2c_master_init(&s_dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&s_dev, 128, 64);
    ssd1306_clear_screen(&s_dev, false);
    ssd1306_contrast(&s_dev, 0xff);
}

void oled_display_status(const char *line1, const char *line2)
{
    ssd1306_clear_screen(&s_dev, false);
    ssd1306_display_text(&s_dev, 0, line1, strlen(line1), false);
    if (line2 != NULL) {
        ssd1306_display_text(&s_dev, 1, line2, strlen(line2), false);
    }
}

void oled_display_summary(fault_summary_t summary)
{
    char line[24];

    ssd1306_clear_screen(&s_dev, false);

    /* Page 0: big-picture verdict */
    if (summary.root_cause > 0) {
        ssd1306_display_text(&s_dev, 0, "FAULT DETECTED", 14, true); /* inverted = stands out */
    } else if (summary.borderline > 0) {
        ssd1306_display_text(&s_dev, 0, "CHECK BORDERLINE", 16, false);
    } else {
        ssd1306_display_text(&s_dev, 0, "BOARD HEALTHY", 13, false);
    }

    /* Pages 2-5: the counts, one per line */
    snprintf(line, sizeof(line), "Root cause : %d", summary.root_cause);
    ssd1306_display_text(&s_dev, 2, line, strlen(line), false);

    snprintf(line, sizeof(line), "Ripple eff : %d", summary.ripple);
    ssd1306_display_text(&s_dev, 3, line, strlen(line), false);

    snprintf(line, sizeof(line), "Borderline : %d", summary.borderline);
    ssd1306_display_text(&s_dev, 4, line, strlen(line), false);

    snprintf(line, sizeof(line), "Healthy    : %d", summary.healthy);
    ssd1306_display_text(&s_dev, 5, line, strlen(line), false);
}

void oled_display_fault_details(const fault_detail_t *details, int count, int delay_ms)
{
    if (details == NULL || count <= 0) return;

    for (int i = 0; i < count; i++) {
        char header[32];
        char line2[24];

        ssd1306_clear_screen(&s_dev, false);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(header, sizeof(header), "%d/%d %s", i + 1, count, details[i].status);
#pragma GCC diagnostic pop
        ssd1306_display_text(&s_dev, 0, header, strlen(header), true); /* inverted header */

        ssd1306_display_text(&s_dev, 2, details[i].node_pair, strlen(details[i].node_pair), false);

        snprintf(line2, sizeof(line2), "(%s)", details[i].type_str);
        ssd1306_display_text(&s_dev, 3, line2, strlen(line2), false);

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}
