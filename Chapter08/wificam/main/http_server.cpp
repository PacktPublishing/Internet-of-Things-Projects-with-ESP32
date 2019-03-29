#include "lwip/api.h"
#include "camera.h"
#include "bitmap.h"
#include "iot_lcd.h"
#include "app_camera.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_system.h"
#include "nvs_flash.h"

typedef struct {
    uint8_t frame_num;
} camera_evt_t;

QueueHandle_t camera_queue = NULL;


static const char* TAG = "WIFI-CAM";

// camera code
const static char http_hdr[] = "HTTP/1.1 200 OK\r\n";
const static char http_bitmap_hdr[] = "Content-type: image\r\n\r\n";

void queue_send(uint8_t frame_num)
{
    camera_evt_t camera_event;
    camera_event.frame_num = frame_num;
    xQueueSend(camera_queue, &camera_event, portMAX_DELAY);
}

uint8_t queue_receive()
{
    camera_evt_t camera_event;
    xQueueReceive(camera_queue, &camera_event, portMAX_DELAY);
    return camera_event.frame_num;
}

uint8_t queue_available()
{
    return uxQueueSpacesAvailable(camera_queue);
}

void camera_queue_init()
{
    camera_queue = xQueueCreate(CAMERA_CACHE_NUM - 1, sizeof(camera_evt_t));
}

inline uint8_t unpack(int byteNumber, uint32_t value)
{
    return (value >> (byteNumber * 8));
}

void convert_fb32bit_line_to_bmp565(uint32_t *srcline, uint8_t *destline,
        const camera_pixelformat_t format)
{
    uint16_t pixel565 = 0;
    uint16_t pixel565_2 = 0;
    uint32_t long2px = 0;
    uint16_t *sptr;
    uint16_t conver_times;
    uint16_t current_src_pos = 0, current_dest_pos = 0;

    switch (CAMERA_FRAME_SIZE) {
        case CAMERA_FS_SVGA:
            conver_times = 800;
            break;
        case CAMERA_FS_VGA:
            conver_times = 640;
            break;
        case CAMERA_FS_QVGA:
            conver_times = 320;
            break;
        case CAMERA_FS_QCIF:
            conver_times = 176;
            break;
        case CAMERA_FS_HQVGA:
            conver_times = 240;
            break;
        case CAMERA_FS_QQVGA:
            conver_times = 160;
            break;
        default:
            printf("framesize not init\n");
            break;
    }

    for (int current_pixel_pos = 0; current_pixel_pos < conver_times;
            current_pixel_pos += 2) {
        current_src_pos = current_pixel_pos / 2;
        long2px = srcline[current_src_pos];

        pixel565 = (unpack(2, long2px) << 8) | unpack(3, long2px);
        pixel565_2 = (unpack(0, long2px) << 8) | unpack(1, long2px);

        sptr = (uint16_t *) &destline[current_dest_pos];
        *sptr = pixel565;
        sptr = (uint16_t *) &destline[current_dest_pos + 2];
        *sptr = pixel565_2;
        current_dest_pos += 4;
    }
}

void http_server_netconn_serve(struct netconn *conn, uint8_t frame_num)
{
    struct netbuf *inbuf;
    char *buf;
    u16_t buflen;
    err_t err;
    err = netconn_recv(conn, &inbuf);
    if (err == ERR_OK) {
        netbuf_data(inbuf, (void**) &buf, &buflen);
        ESP_LOGI(TAG, "netbuf_data stop :%d, buflen: %d, buf:\n%s\n", xTaskGetTickCount(), buflen, buf);
        if (buflen >= 5 && buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T'
                && buf[3] == ' ' && buf[4] == '/') {
            ESP_LOGD(TAG, "Start Image Sending...");
            netconn_write(conn, http_hdr, sizeof(http_hdr) - 1, NETCONN_NOCOPY);
            netconn_write(conn, http_bitmap_hdr, sizeof(http_bitmap_hdr) - 1, NETCONN_NOCOPY);
            if (memcmp(&buf[5], "pic", 3) == 0) {
                char *bmp = bmp_create_header565(camera_get_fb_width(),
                        camera_get_fb_height());
                err = netconn_write(conn, bmp, sizeof(bitmap565), NETCONN_COPY);
                free(bmp);
            }

            if ((CAMERA_PIXEL_FORMAT == CAMERA_PF_RGB565)
                    || (CAMERA_PIXEL_FORMAT == CAMERA_PF_YUV422)) {
                uint8_t s_line[camera_get_fb_width() * 2]; //camera_get_fb_width()
                uint32_t *fbl;
                uint32_t *currFbPtr = camera_get_fb(frame_num);
                for (int i = 0; i < camera_get_fb_height(); i++) { //camera_get_fb_height()
                    fbl = (uint32_t *) &currFbPtr[(i * camera_get_fb_width()) / 2]; //(i*(320*2)/4); // 4 bytes for each 2 pixel / 2 byte read..
                    convert_fb32bit_line_to_bmp565(fbl, s_line, CAMERA_PIXEL_FORMAT);
                    err = netconn_write(conn, s_line, camera_get_fb_width() * 2, NETCONN_COPY);
                }
                ESP_LOGD(TAG, "Image sending Done ...");
            } else {
                err = netconn_write(conn, camera_get_fb(0), camera_get_data_size(), NETCONN_NOCOPY);
            }

        }  // end GET request:
    }
    netconn_close(conn); 
    netbuf_delete(inbuf);
}

void http_server_task(void *pvParameters)
{
    uint8_t i = 0;
    struct netconn *conn, *newconn;
    err_t err, ert;
    conn = netconn_new(NETCONN_TCP); /* creat TCP connector */
    netconn_bind(conn, NULL, 80); /* bind HTTP port */
    netconn_listen(conn); /* server listen connect */
    do {
        ESP_LOGI(TAG, "netconn_accept start :%d\n", xTaskGetTickCount());
        err = netconn_accept(conn, &newconn);
        if (err == ERR_OK) { /* new conn is coming */

            http_server_netconn_serve(newconn, queue_receive());

            ESP_LOGI(TAG, "http_server->xSemaphoreGive:::%d\n", i++);

            netconn_delete(newconn);
        }
    } while (err == ERR_OK);
    netconn_close(conn);
    netconn_delete(conn);
}
