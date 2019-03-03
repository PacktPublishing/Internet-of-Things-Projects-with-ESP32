#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>

#include "tcpip_adapter.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "freertos/event_groups.h"

#include <esp_http_server.h>


static const char *TAG="SMARTMOBILE";
static EventGroupHandle_t wifi_event_group;
static const int CONNECTED_BIT = BIT0;
static const int WIFI_INIT_DONE_BIT = BIT1;

#define LAMP1 12
#define LAMP2 14
#define LAMP3 26

int lamp1_state, lamp2_state, lamp3_state;


esp_err_t ping_get_handler(httpd_req_t *req)
{   
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    return ESP_OK;
}

httpd_uri_t ping = {
    .uri       = "/ping",
    .method    = HTTP_GET,
    .handler   = ping_get_handler,
    .user_ctx  = "pong!"
};

esp_err_t state_get_handler(httpd_req_t *req)
{   
    char buf[15];
    
    sprintf(buf,"1:%d,2:%d,3:%d",lamp1_state,lamp2_state,lamp3_state);
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

httpd_uri_t state = {
    .uri       = "/state",
    .method    = HTTP_GET,
    .handler   = state_get_handler,
    .user_ctx  = NULL
};

/* An HTTP POST handler */
esp_err_t lamp_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    // 1: turn on lamp1
    // 2: turn off lamp1
    // 3: turn on lamp2
    // 4: turn off lamp2
    // 5: turn on lamp3
    // 6: turn off lamp3

    while (remaining > 0) {
        buf[0] = '\0';
        if ((ret = httpd_req_recv(req, &buf, 1)) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        ESP_LOGI(TAG, "Recv HTTP => %s", buf);
        switch(buf[0]){
            case '1':
                ESP_LOGI(TAG, ">>> Turn on LAMP 1");
                gpio_set_level(LAMP1, 1);
                sprintf(buf,"Turn on LAMP 1");
                httpd_resp_send_chunk(req, buf, strlen(buf));
                lamp1_state = 1;
                break;
            case '2':
                ESP_LOGI(TAG, ">>> Turn on LAMP 1");
                gpio_set_level(LAMP1, 0);
                sprintf(buf,"Turn off LAMP 1");
                httpd_resp_send_chunk(req, buf, strlen(buf));
                lamp1_state = 0;
                break;
            case '3':
                ESP_LOGI(TAG, ">>> Turn on LAMP 2");
                gpio_set_level(LAMP2, 1);
                sprintf(buf,"Turn on LAMP 2");
                httpd_resp_send_chunk(req, buf, strlen(buf));
                lamp2_state = 1;
                break;
            case '4':
                ESP_LOGI(TAG, ">>> Turn off LAMP 2");
                gpio_set_level(LAMP2, 0);
                sprintf(buf,"Turn off LAMP 2");
                httpd_resp_send_chunk(req, buf, strlen(buf));
                lamp2_state = 0;
                break;
            case '5':
                ESP_LOGI(TAG, ">>> Turn on LAMP 3");
                gpio_set_level(LAMP3, 1);
                sprintf(buf,"Turn on LAMP 3");
                httpd_resp_send_chunk(req, buf, strlen(buf));
                lamp3_state = 1;
                break;
            case '6':
                ESP_LOGI(TAG, ">>> Turn off LAMP 3");
                gpio_set_level(LAMP3, 0);
                sprintf(buf,"Turn off LAMP 3");
                httpd_resp_send_chunk(req, buf, strlen(buf));
                lamp3_state = 0;
                break;
            default:
                ESP_LOGI(TAG, ">>> Invalid request");   
                sprintf(buf,"Invalid request");
                httpd_resp_send_chunk(req, buf, strlen(buf));                             
                break;
        }
        remaining -= ret;
       
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

httpd_uri_t lamp_post = {
    .uri       = "/lamp",
    .method    = HTTP_POST,
    .handler   = lamp_post_handler,
    .user_ctx  = NULL
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
        httpd_register_uri_handler(server, &state);
        httpd_register_uri_handler(server, &lamp_post);
        httpd_register_uri_handler(server, &ping);
        
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

    switch (event->event_id) {
        case SYSTEM_EVENT_AP_START:
            xEventGroupSetBits(wifi_event_group, WIFI_INIT_DONE_BIT);
            ESP_LOGI(TAG, "ap init down");
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            ESP_LOGI(TAG, "sta connect");

            /* Start the web server */
            if (*server == NULL) {
                *server = start_webserver();
            }
            break;
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
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

void initialize_wifi(void *arg)
{
    ESP_LOGI(TAG, "initialize WiFi");

    tcpip_adapter_ip_info_t ip_info;
    //ESP_ERROR_CHECK(nvs_flash_init());
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
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, arg));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );

    wifi_config_t wifi_config;
    memcpy(wifi_config.ap.ssid, "SMART-MOBILE", sizeof("SMART-MOBILE"));
    memcpy(wifi_config.ap.password, "123456789", sizeof("123456789"));
    wifi_config.ap.ssid_len = strlen("SMART-MOBILE");
    wifi_config.ap.max_connection = 1;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    esp_wifi_start();
}

static void initialize_gpio(){

    ESP_LOGI(TAG, "initialize GPIO");
    // set gpio and its direction
    gpio_pad_select_gpio(LAMP1);    
    gpio_set_direction(LAMP1, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(LAMP2);    
    gpio_set_direction(LAMP2, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(LAMP3);    
    gpio_set_direction(LAMP3, GPIO_MODE_OUTPUT);

    // turn off lamps 
    gpio_set_level(LAMP1, 0);
    gpio_set_level(LAMP2, 0);
    gpio_set_level(LAMP3, 0);

    lamp1_state = 0;
    lamp2_state = 0;
    lamp3_state = 0;
}

void app_main()
{
    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(nvs_flash_init());
    initialize_gpio();
    initialize_wifi(&server);

}
