#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define LED1 12
#define LED2 14
#define LED3 26

void turn_on_led(int led)
{
    // turn off all leds
    gpio_set_level(LED1, 0);
    gpio_set_level(LED2, 0);
    gpio_set_level(LED3, 0);

    switch(led)
    {
        case 1:
            gpio_set_level(LED1, 1);
            break;
        case 2:
            gpio_set_level(LED2, 1);
            break;
        case 3:
            gpio_set_level(LED3, 1);
            break;
    }
}

void blinking_task(void *pvParameter)
{
    // set gpio and its direction
    gpio_pad_select_gpio(LED1);    
    gpio_set_direction(LED1, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(LED2);    
    gpio_set_direction(LED2, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(LED3);    
    gpio_set_direction(LED3, GPIO_MODE_OUTPUT);

    int current_led = 1;
    while(1) {
        turn_on_led(current_led);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        
        current_led++;
        if(current_led>3)
            current_led = 1;
    }
}

void app_main()
{
    xTaskCreate(&blinking_task, "blinking_task", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
}