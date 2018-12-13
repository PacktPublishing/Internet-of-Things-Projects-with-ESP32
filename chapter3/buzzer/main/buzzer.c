#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define BUZZER 27

void buzzer_task(void *pvParameter)
{
    // set gpio and its direction
    gpio_pad_select_gpio(BUZZER);    
    gpio_set_direction(BUZZER, GPIO_MODE_OUTPUT);    

    int sounding = 1;
    while(1) {

        if(sounding==1){
            gpio_set_level(BUZZER, 1);
            sounding = 0;
        }            
        else {
            gpio_set_level(BUZZER, 0);
            sounding = 1;
        }
            
        vTaskDelay(1000 / portTICK_PERIOD_MS);
                
    }
}

void app_main()
{
    xTaskCreate(&buzzer_task, "buzzer_task", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
}