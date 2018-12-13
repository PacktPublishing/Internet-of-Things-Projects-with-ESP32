#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include <dht.h>
static const dht_sensor_type_t sensor_type = DHT_TYPE_DHT22;
static const gpio_num_t dht_gpio = 26;


void dht_task(void *pvParameter)
{
    int16_t temperature = 0;
    int16_t humidity = 0;
    
    while(1) {
        if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK)
            printf("Humidity: %d%% Temp: %d^C\n", humidity / 10, temperature / 10);
        else
            printf("Could not read data from sensor\n");

        vTaskDelay(5000 / portTICK_PERIOD_MS);
        
    }
}

void app_main()
{
    xTaskCreate(&dht_task, "dht_task", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
}