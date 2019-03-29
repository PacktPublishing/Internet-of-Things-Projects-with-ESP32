/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */
#include "lwip/api.h"
#include "camera.h"
#include "bitmap.h"
#include "iot_lcd.h"
#include "esp_event_loop.h"
#include "app_camera.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

static const char *TAG = "WIFI-CAM";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int WIFI_INIT_DONE_BIT = BIT1;

void app_camera_init()
{
    camera_model_t camera_model;
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CONFIG_D0;
    config.pin_d1 = CONFIG_D1;
    config.pin_d2 = CONFIG_D2;
    config.pin_d3 = CONFIG_D3;
    config.pin_d4 = CONFIG_D4;
    config.pin_d5 = CONFIG_D5;
    config.pin_d6 = CONFIG_D6;
    config.pin_d7 = CONFIG_D7;
    config.pin_xclk = CONFIG_XCLK;
    config.pin_pclk = CONFIG_PCLK;
    config.pin_vsync = CONFIG_VSYNC;
    config.pin_href = CONFIG_HREF;
    config.pin_sscb_sda = CONFIG_SDA;
    config.pin_sscb_scl = CONFIG_SCL;
    config.pin_reset = CONFIG_RESET;
    config.xclk_freq_hz = CONFIG_XCLK_FREQ;

    //Warning: This gets squeezed into IRAM.
    volatile static uint32_t * * currFbPtr __attribute__ ((aligned(4))) = NULL;
    currFbPtr = (volatile uint32_t **)malloc(sizeof(uint32_t *) * CAMERA_CACHE_NUM);

    ESP_LOGI(TAG, "get free size of 32BIT heap : %d\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));

    for (int i = 0; i < CAMERA_CACHE_NUM; i++) {
        currFbPtr[i] = (volatile uint32_t *) heap_caps_malloc(320 * 240 * 2, MALLOC_CAP_32BIT);
        ESP_LOGI(TAG, "framebuffer address is:%p\n", currFbPtr[i]);
    }

    ESP_LOGI(TAG, "%s\n", currFbPtr[0] == NULL ? "currFbPtr is NULL" : "currFbPtr not NULL");
    ESP_LOGI(TAG, "framebuffer address is:%p\n", currFbPtr[0]);

    // camera init
    esp_err_t err = camera_probe(&config, &camera_model);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera probe failed with error 0x%x", err);
        return;
    }

    if (camera_model == CAMERA_OV7670) {
        ESP_LOGI(TAG, "Detected OV7670 camera");
        config.frame_size = CAMERA_FRAME_SIZE;
    } else {
        ESP_LOGI(TAG, "Cant detected ov7670 camera");
    }

    config.displayBuffer = (uint32_t **) currFbPtr;
    config.pixel_format = CAMERA_PIXEL_FORMAT;
    config.test_pattern_enabled = 0;

    err = camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return;
    }
}


static void app_camera_task(void *pvParameters)
{
    while (1) {
        queue_send(camera_run() % CAMERA_CACHE_NUM);

    }
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_AP_START:
        xEventGroupSetBits(wifi_event_group, WIFI_INIT_DONE_BIT);
        ESP_LOGI(TAG, "ap init down");
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        ESP_LOGI(TAG, "sta connect");
        break;
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        break;
    default:
        break;
    }
    return ESP_OK;
}

void initialize_wifi(void)
{
    tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(nvs_flash_init());
    // set TCP range
    tcpip_adapter_init();
    tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
    ip_info.ip.addr = inet_addr("192.168.0.1");
    ip_info.gw.addr = inet_addr("192.168.0.0");
    tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
    tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
    // wifi init
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );

    wifi_config_t wifi_config;
    memcpy(wifi_config.ap.ssid, "WIFI-CAM", sizeof("WIFI-CAM"));
    memcpy(wifi_config.ap.password, "123456789", sizeof("123456789"));
    wifi_config.ap.ssid_len = strlen("WIFI-CAM");
    wifi_config.ap.max_connection = 1;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    esp_wifi_start();
}

extern "C" void app_main()
{
    app_camera_init();
    camera_queue_init();

    initialize_wifi();
    tcpip_adapter_ip_info_t ipconfig;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ipconfig);
    ip4_addr_t s_ip_addr = ipconfig.ip;
    
    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Free heap: %u", xPortGetFreeHeapSize());

    ESP_LOGD(TAG, "Starting app_camera_task...");
    xTaskCreatePinnedToCore(&app_camera_task, "app_camera_task", 4096, NULL, 3, NULL, 1);

    xEventGroupWaitBits(wifi_event_group, WIFI_INIT_DONE_BIT, true, false, portMAX_DELAY);
    ESP_LOGD(TAG, "Starting http_server task...");
    xTaskCreatePinnedToCore(&http_server_task, "http_server_task", 4096, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "open http://" IPSTR "/pic for single image/bitmap image", IP2STR(&s_ip_addr));    

    ESP_LOGI(TAG, "get free size of 32BIT heap : %d\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    ESP_LOGI(TAG, "task stack: %d", uxTaskGetStackHighWaterMark(NULL));
    ESP_LOGI(TAG, "Camera demo ready.");
}
