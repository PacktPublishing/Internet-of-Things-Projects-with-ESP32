#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_http_server.h>

#include <dht.h>
static const dht_sensor_type_t sensor_type = DHT_TYPE_DHT22;
static const gpio_num_t dht_gpio = 26;

static const char *TAG="APP";
static const char *WEATHER_TXT =
"<html>"
"<head><title>%s</title></head>"
"<body>"
"<p>Temperature: %d </p>"
"<p>Humidity: %d %%</p>"
"</body>"
"</html>";


static const char *WEATHER_TXT_REF =
"<html>"
"<head><title>%s</title>"
"<meta http-equiv=\"refresh\" content=\"5\" >"
"</head>"
"<body>"
"<p>Temperature: %d </p>"
"<p>Humidity: %d %%</p>"
"</body>"
"</html>";



esp_err_t weather_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Request headers lost");

    int16_t temperature = 0;
    int16_t humidity = 0;
    char tmp_buff[256];

    if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK)
    {        
        //sprintf(tmp_buff, WEATHER_TXT, (const char*) req->user_ctx, temperature/10, humidity/10);
        sprintf(tmp_buff, WEATHER_TXT_REF, (const char*) req->user_ctx, temperature/10, humidity/10);
        	
    }else{
        tmp_buff[0]='\0';
    }
    
    httpd_resp_send(req, tmp_buff, strlen(tmp_buff));

    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

httpd_uri_t weather = {
    .uri       = "/weather",
    .method    = HTTP_GET,
    .handler   = weather_get_handler,
    .user_ctx  = "ESP32 Weather System"
};

esp_err_t temperature_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Request headers lost");

    int16_t temperature = 0;
    int16_t humidity = 0;
    char tmp_buff[10];

    if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK)
    {                
        sprintf(tmp_buff, "%d",temperature/10);
        	
    }else{
        tmp_buff[0]='\0';
    }
    
    httpd_resp_send(req, tmp_buff, strlen(tmp_buff));

    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

httpd_uri_t weather_temp = {
    .uri       = "/temp",
    .method    = HTTP_GET,
    .handler   = temperature_get_handler,
    .user_ctx  = "ESP32 Weather System"
};



httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &weather);
        httpd_register_uri_handler(server, &weather_temp);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    httpd_handle_t *server = (httpd_handle_t *) ctx;

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        ESP_LOGI(TAG, "Got IP: '%s'",
                ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

        /* Start the web server */
        if (*server == NULL) {
            *server = start_webserver();
        }
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
        ESP_ERROR_CHECK(esp_wifi_connect());

        /* Stop the web server */
        if (*server) {
            stop_webserver(*server);
            *server = NULL;
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialize_wifi(void *arg)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, arg));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "SSID",
            .password = "SSID_KEY",
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main()
{
    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(nvs_flash_init());
    initialize_wifi(&server);
}
