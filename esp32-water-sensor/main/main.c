#include <stdio.h>
#include "esp_log.h"
#define LOG_TAG "main"
#define LOG_LEVEL_LOCAL ESP_LOG_INFO

#include "driver/gpio.h"
#include "driver/adc.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#define DEFAULT_SSID "WIFISSID"
#define DEFAULT_PWD "WIFIPASSWORD"
#define WIFI_MAXIMUM_RETRY 5


#include "esp_websocket_client.h"
#define WS_ADDRESS "ws://<IP_ADDRESS>:<PORT>"

#include "esp_timer.h"

// Resend message if no acknowledgement received in this interval
// Set in microseconds (60000000 us = 1 minute); multiply by number for minutes 
#define RESEND_NO_ACK_INTERVAL 60000000 * 5
// Reset send time for sending sensor value to websocket, in microseconds
// Will send sensor value every 15 minutes no matter what (60000000 * 15)
#define SEND_INFO_INTERVAL 60000000 * 15

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

int sensor_value = 0;
int sensor_threshold = 1800;
int debounce_ms = 0;
bool sensor_notification_ack = false;

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(LOG_TAG, "Connected to AP");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(LOG_TAG, "Retrying connection to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(LOG_TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void){
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = DEFAULT_SSID,
            .password = DEFAULT_PWD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 DEFAULT_SSID, DEFAULT_PWD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 DEFAULT_SSID, DEFAULT_PWD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){
    ESP_LOGI(LOG_TAG, "Websocket event: %d", (int) event_id);
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch(event_id){
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(LOG_TAG, "Websocket connected");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(LOG_TAG, "Websocket disconnected");
            break;
        case WEBSOCKET_EVENT_DATA:            
            //ignore pong messages
            if (data->op_code == 0xA) break;
            ESP_LOGI(LOG_TAG, "Websocket data received");
            char * buffer;
            buffer = (char *) malloc((data->payload_len + 1) * sizeof(char));
            strncpy(buffer, data->data_ptr, data->payload_len);
            buffer[data->payload_len] = '\0';
            
            int new_threshold = atoi(buffer);
            if (new_threshold != 0){
                ESP_LOGI(LOG_TAG, "Setting threshold to %d", (int) new_threshold);
                sensor_threshold = new_threshold;
            }
            if (strncmp(data->data_ptr, "ack", 3) == 0){
                sensor_notification_ack = true;
                ESP_LOGI(LOG_TAG, "Sensor notification acknowledged");
            }
            free(buffer);
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGI(LOG_TAG, "Websocket error");
            break;
    }
}

static void send_sensor_value(esp_websocket_client_handle_t client){
    sensor_notification_ack = false;

    ESP_LOGI(LOG_TAG, "Sending sensor value: %d", sensor_value);
    char sensor_value_str[10];
    sprintf(sensor_value_str, "%d", (int) sensor_value);
    esp_websocket_client_send_text(client, sensor_value_str, strlen(sensor_value_str), portMAX_DELAY);
}

static esp_websocket_client_handle_t websocket_start(void){
    esp_websocket_client_config_t ws_cfg = {
        .uri = WS_ADDRESS,
    };

    ESP_LOGI(LOG_TAG, "Connecting to websocket server at %s...", WS_ADDRESS);
    esp_websocket_client_handle_t client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    esp_websocket_client_start(client);
    
    return client;
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    
    wifi_init_sta();

    esp_websocket_client_handle_t client = websocket_start();

    debounce_ms = esp_timer_get_time();
    while(1){
        sensor_value = adc1_get_raw(ADC1_CHANNEL_0);
        vTaskDelay(1);
        if (sensor_value > sensor_threshold){
            if (!sensor_notification_ack && ((esp_timer_get_time() - debounce_ms) > RESEND_NO_ACK_INTERVAL)){
                send_sensor_value(client);
                debounce_ms = esp_timer_get_time();
            }
            else if ((esp_timer_get_time() - debounce_ms) > SEND_INFO_INTERVAL){
                send_sensor_value(client);
                debounce_ms = esp_timer_get_time();
            }
        }

    }
}