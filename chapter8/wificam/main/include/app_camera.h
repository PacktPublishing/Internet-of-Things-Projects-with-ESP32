#ifndef _IOT_CAMERA_TASK_H_
#define _IOT_CAMERA_TASK_H_

#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD
#define WIFI_SSID     CONFIG_WIFI_SSID
/**
 * CAMERA_PF_RGB565 = 0,       //!< RGB, 2 bytes per pixel
 * CAMERA_PF_YUV422 = 1,       //!< YUYV, 2 bytes per pixel
 * CAMERA_PF_GRAYSCALE = 2,    //!< 1 byte per pixel
 * CAMERA_PF_JPEG = 3,         //!< JPEG compressed
 * CAMERA_PF_RGB555 = 4,       //!< RGB, 2 bytes per pixel
 * CAMERA_PF_RGB444 = 5,       //!< RGB, 2 bytes per pixel
 */
#define CAMERA_PIXEL_FORMAT CAMERA_PF_RGB565

/*
 * CAMERA_FS_QQVGA = 4,     //!< 160x120
 * CAMERA_FS_HQVGA = 7,     //!< 240x160
 * CAMERA_FS_QCIF = 6,      //!< 176x144
 * CAMERA_FS_QVGA = 8,      //!< 320x240
 * CAMERA_FS_VGA = 10,      //!< 640x480
 * CAMERA_FS_SVGA = 11,     //!< 800x600
 */
#define CAMERA_FRAME_SIZE CAMERA_FS_QVGA

#define RGB565_MASK_RED        0xF800
#define RGB565_MASK_GREEN      0x07E0
#define RGB565_MASK_BLUE       0x001F

/**
 * @breif call xSemaphoreTake to receive camera frame number
 */
uint8_t queue_receive();
void camera_queue_init();

/**
 * @breif call xSemaphoreGive to send camera frame number
 */
void queue_send(uint8_t frame_num);

uint8_t queue_available();

void lcd_init_wifi(void);

void lcd_camera_init_complete(void);

void lcd_wifi_connect_complete(void);

void lcd_http_info(ip4_addr_t s_ip_addr);

void app_lcd_init(void);

void app_lcd_task(void *pvParameters);

void http_server_task(void *pvParameters);

#endif
