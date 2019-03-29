#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

esp_err_t event_handler(void *ctx, system_event_t *event)
{
   if (event->event_id == SYSTEM_EVENT_SCAN_DONE) {
      uint16_t apCount = 0;
      esp_wifi_scan_get_ap_num(&apCount);
      printf("WiFi found: %d\n",event->event_info.scan_done.number);
      if (apCount == 0) {
         return ESP_OK;
      }
      wifi_ap_record_t *wifi = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
      ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, wifi));
      for (int i=0; i<apCount; i++) {
         char *authmode;
         switch(wifi[i].authmode) {
            case WIFI_AUTH_OPEN:
               authmode = "NO AUTH";
               break;
            case WIFI_AUTH_WEP:
               authmode = "WEP";
               break;           
            case WIFI_AUTH_WPA_PSK:
               authmode = "WPA PSK";
               break;           
            case WIFI_AUTH_WPA2_PSK:
               authmode = "WPA2 PSK";
               break;           
            case WIFI_AUTH_WPA_WPA2_PSK:
               authmode = "WPA/WPA2 PSK";
               break;
            default:
               authmode = "Unknown";
               break;
         }
         printf("SSID: %15.15s RSSI: %4d AUTH: %10.10s\n",wifi[i].ssid, wifi[i].rssi, authmode);
      }
      free(wifi);
      printf("\n\n");
   }
   return ESP_OK;
}

int app_main(void)
{
   esp_err_t ret = nvs_flash_init();
   if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
   }
   ESP_ERROR_CHECK( ret );

   tcpip_adapter_init();
   ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
   ESP_ERROR_CHECK(esp_wifi_init(&cfg));
   ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
   ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
   ESP_ERROR_CHECK(esp_wifi_start());
   
   wifi_scan_config_t scanConf = {
      .ssid = NULL,
      .bssid = NULL,
      .channel = 0,
      .show_hidden = true
   };

   while(true){
       ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));                                                                   
       vTaskDelay(3000 / portTICK_PERIOD_MS);
   }
   

   return 0;
}