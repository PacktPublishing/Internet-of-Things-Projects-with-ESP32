#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/uart.h"

// minmea
#include "minmea.h"


static const char *TAG = "GPS";

/* Which UART is the GPS module on? */
#define GPS_UART_NUM      UART_NUM_1
#define GPS_UART_RX_PIN  (22)
#define GPS_UART_TX_PIN  (23)
#define GPS_UART_RTS_PIN (21)
#define GPS_UART_CTS_PIN (35)

/* Parameters for the data buffers */
#define UART_SIZE (80)
#define UART_RX_BUF_SIZE (1024)

// GPS variables and initial state
float latitude = -1.0;
float longitude = -1.0;
int fix_quality = -1;
int satellites_tracked = -1;


esp_err_t wifi_scan_event_handler(void *ctx, system_event_t *event)
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
         printf("Lat: %f Long: %f SSID: %15.15s RSSI: %4d AUTH: %10.10s\n",latitude, longitude, 
                wifi[i].ssid, wifi[i].rssi, authmode);
      }
      free(wifi);
      printf("\n\n");
   }
   return ESP_OK;
}
// read a line from the UART controller
char* read_line(uart_port_t uart_controller) {

	static char line[UART_SIZE];
	char *ptr = line;
	while(1) {
	
		int num_read = uart_read_bytes(uart_controller, (unsigned char *)ptr, 1, portMAX_DELAY);
		if(num_read == 1) {
		
			// new line found, terminate the string and return
			if(*ptr == '\n') {
				ptr++;
				*ptr = '\0';
				return line;
			}
			
			// else move to the next char
			ptr++;
		}
	}
}
void parse_gps_nmea(char* line){
    // parse the line    
    switch (minmea_sentence_id(line, false)) {
        
        case MINMEA_SENTENCE_RMC: {
            
            struct minmea_sentence_rmc frame;
            if (minmea_parse_rmc(&frame, line)) {
                
                // latitude valid and changed? apply a threshold
                float new_latitude = minmea_tocoord(&frame.latitude);
                if((new_latitude != NAN) && (abs(new_latitude - latitude) > 0.001)) {
                    latitude = new_latitude;
                    printf("New latitude: %f\n", latitude);
                }
                
                // longitude valid and changed? apply a threshold
                float new_longitude = minmea_tocoord(&frame.longitude);
                if((new_longitude != NAN) && (abs(new_longitude - longitude) > 0.001)) {
                    longitude = minmea_tocoord(&frame.longitude);
                    printf("New longitude: %f\n", longitude);
                }
            }
        } break;

        case MINMEA_SENTENCE_GGA: {
            
            struct minmea_sentence_gga frame;
            if (minmea_parse_gga(&frame, line)) {
                
                // fix quality changed?
                if(frame.fix_quality != fix_quality) {
                    fix_quality = frame.fix_quality;
                    printf("New fix quality: %d\n", fix_quality);
                }
            }
        } break;

        case MINMEA_SENTENCE_GSV: {
            
            struct minmea_sentence_gsv frame;
            if (minmea_parse_gsv(&frame, line)) {

                // number of satellites changed?
                if(frame.total_sats != satellites_tracked) {
                    satellites_tracked = frame.total_sats;
                    printf("New satellites tracked: %d\n", satellites_tracked);
                }
            }
        } break;
        
        default: break;
    }
}


static void uart_event_task(void *pvParameters)
{
    while (1) {        
        char *line = read_line(GPS_UART_NUM);
        
        parse_gps_nmea(line);
 
    }
    /* Should never get here */
    vTaskDelete(NULL);
}

static void config_gps_uart(void) {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(GPS_UART_NUM, &uart_config);
    //Set UART pins (using UART0 default pins ie no changes.)
    uart_set_pin(GPS_UART_NUM, GPS_UART_RX_PIN, GPS_UART_TX_PIN, GPS_UART_RTS_PIN, GPS_UART_CTS_PIN);
    //Install UART driver    
    uart_driver_install(GPS_UART_NUM, UART_RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
        
}
static void config_wifi(void) {
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_scan_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}
int app_main(void)
{
    ESP_LOGI(TAG, "Configuring flash");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    config_wifi();
    

    ESP_LOGI(TAG, "Configuring UART");
    config_gps_uart();
    
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