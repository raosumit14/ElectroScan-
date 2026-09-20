#include "hardware.h"

void gpio_init(void)
{
    gpio_config_t io_conf =
    {
        .mode = GPIO_MODE_OUTPUT,

        .pin_bit_mask =

            (1ULL << MUX1_A) |
            (1ULL << MUX1_B) |
            (1ULL << MUX1_C) |
         //   (1ULL << MUX1_INH) |

            (1ULL << MUX2_A) |
            (1ULL << MUX2_B) |
            (1ULL << MUX2_C) |

            (1ULL << MUX3_A) |
            (1ULL << MUX3_B) |
            (1ULL << MUX3_C) |

            (1ULL << DEMUX_A) |
            (1ULL << DEMUX_B) |
            (1ULL << DEMUX_C) |
            (1ULL << DEMUX_INH) |

            (1ULL << RES_ENABLE),

        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);

    /* Initial States */

   // gpio_set_level(MUX1_INH, 0);      // Enable MUX1
    gpio_set_level(DEMUX_INH, 1);     // Disable DEMUX
    gpio_set_level(RES_ENABLE, 0);    // Resistance circuit OFF
}

void adc_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);

    adc1_config_channel_atten(
            RES_ADC_CHANNEL,
            ADC_ATTEN_DB_11);

    adc1_config_channel_atten(
            VOLT_ADC_CHANNEL,
            ADC_ATTEN_DB_11);
}