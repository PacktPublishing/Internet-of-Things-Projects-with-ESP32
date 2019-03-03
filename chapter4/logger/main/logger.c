#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp32/ulp.h"
#include "driver/touch_pad.h"
#include "driver/adc.h"
#include "driver/rtc_io.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include <dht.h>
#include "sdkconfig.h"


static RTC_DATA_ATTR struct timeval sleep_enter_time;
static const dht_sensor_type_t sensor_type = DHT_TYPE_DHT22;
static const gpio_num_t dht_gpio = 26;

void app_main()
{
    // initialize SDMMC
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
    gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
    gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

    int16_t temperature = 0;
    int16_t humidity = 0;

    struct timeval now;
    gettimeofday(&now, NULL);
    int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;

    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER: {
            printf("Wake up from timer. Time spent in deep sleep: %dms\n", sleep_time_ms);

            // Options for mounting the filesystem.
            esp_vfs_fat_sdmmc_mount_config_t mount_config = {
                .format_if_mount_failed = false,
                .max_files = 5,
                .allocation_unit_size = 16 * 1024
            };

            sdmmc_card_t* card;
            esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

            if (ret != ESP_OK) {
                if (ret == ESP_FAIL) {
                    printf("Failed to mount filesystem. "
                        "If you want the card to be formatted, set format_if_mount_failed = true.\n");
                } else {
                    printf("Failed to initialize the card (%s). "
                        "Make sure SD card lines have pull-up resistors in place.\n", esp_err_to_name(ret));
                }
                return;
            }

            // Card has been initialized, print its properties
            sdmmc_card_print_info(stdout, card);

           printf("Reading sensor data\n");

            if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK)
            {
                printf("Humidity: %d%% Temp: %d^C\n", humidity / 10, temperature / 10);
                FILE* f = fopen("/sdcard/logger.csv", "a");
                if (f == NULL) {
                    printf("Failed to open file for writing\n");
                    return;
                }                   
                fprintf(f, "%d,%d\n", humidity / 10, temperature / 10);            
                fclose(f);
            }            
            else
                printf("Could not read data from sensor\n");



            // All done, unmount partition and disable SDMMC or SPI peripheral
            esp_vfs_fat_sdmmc_unmount();
            printf("Card unmounted\n");  

            break;
        }
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            printf("Not a deep sleep reset\n");
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    const int wakeup_time_sec = 20;
    printf("Enabling timer wakeup, %ds\n", wakeup_time_sec);
    esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);

    esp_deep_sleep_start();
}
