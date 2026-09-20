#include <stdio.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"

#include "firebase.h"

static const char *TAG = "FIREBASE";

char json_buffer[JSON_BUFFER_SIZE];

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    static int offset = 0;

    switch(evt->event_id)
    {
        case HTTP_EVENT_ON_CONNECTED:

            offset = 0;
            memset(json_buffer,0,sizeof(json_buffer));
            break;

        case HTTP_EVENT_ON_DATA:

            if(!esp_http_client_is_chunked_response(evt->client))
            {
                if(offset + evt->data_len < JSON_BUFFER_SIZE)
                {
                    memcpy(json_buffer + offset,
                           evt->data,
                           evt->data_len);

                    offset += evt->data_len;
                }
            }

            break;

        default:
            break;
    }

    return ESP_OK;
}

esp_err_t firebase_get_netlist(void)
{
    esp_http_client_config_t config =
    {
        .url = FIREBASE_URL,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
         .timeout_ms = 30000,
        .keep_alive_enable = false,
    };

    esp_http_client_handle_t client =
        esp_http_client_init(&config);

    esp_err_t err =
        esp_http_client_perform(client);

    if(err == ESP_OK)
    {
        printf("\n=========== JSON RECEIVED ===========\n");

        printf("%s\n", json_buffer);

        printf("=====================================\n");
    }
    else
    {
        ESP_LOGE(TAG,
                 "HTTP GET Failed : %s",
                 esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    return err;
}