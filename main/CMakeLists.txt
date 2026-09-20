idf_component_register(
    SRCS
        "main.c"
        "hardware.c"
        "resistance.c"
        "voltage.c"
        "firebase.c"
        "wifi.c"
        "json.c"
        "fault_check.c"
        "netlist_bridge.c"
        "fault_check_repeat.c"
        "oled_display.c"

    INCLUDE_DIRS
        "."
    PRIV_REQUIRES "ssd1306"

    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        esp_http_client
        esp-tls
        mbedtls
        json
)