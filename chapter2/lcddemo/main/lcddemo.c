#include <time.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"


// ==========================================================
// Define which spi bus to use TFT_VSPI_HOST or TFT_HSPI_HOST
#define SPI_BUS TFT_HSPI_HOST
// ==========================================================


static int _demo_pass = 0;
static uint8_t doprint = 1;
static struct tm* tm_info;
static char tmp_buff[64];
static time_t time_now, time_last = 0;
static const char *file_fonts[3] = {"/spiffs/fonts/DotMatrix_M.fon", "/spiffs/fonts/Ubuntu.fon", "/spiffs/fonts/Grotesk24x48.fon"};

#define GDEMO_TIME 1000
#define GDEMO_INFO_TIME 5000

//----------------------
static void _checkTime()
{
	time(&time_now);
	if (time_now > time_last) {
		color_t last_fg, last_bg;
		time_last = time_now;
		tm_info = localtime(&time_now);
		sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

		TFT_saveClipWin();
		TFT_resetclipwin();

		Font curr_font = cfont;
		last_bg = _bg;
		last_fg = _fg;
		_fg = TFT_YELLOW;
		_bg = (color_t){ 64, 64, 64 };
		TFT_setFont(DEFAULT_FONT, NULL);

		TFT_fillRect(1, _height-TFT_getfontheight()-8, _width-3, TFT_getfontheight()+6, _bg);
		TFT_print(tmp_buff, CENTER, _height-TFT_getfontheight()-5);

		cfont = curr_font;
		_fg = last_fg;
		_bg = last_bg;

		TFT_restoreClipWin();
	}
}

//---------------------
static int Wait(int ms)
{
	uint8_t tm = 1;
	if (ms < 0) {
		tm = 0;
		ms *= -1;
	}
	if (ms <= 50) {
		vTaskDelay(ms / portTICK_RATE_MS);
		//if (_checkTouch()) return 0;
	}
	else {
		for (int n=0; n<ms; n += 50) {
			vTaskDelay(50 / portTICK_RATE_MS);
			if (tm) _checkTime();
			//if (_checkTouch()) return 0;
		}
	}
	return 1;
}

//-------------------------------------------------------------------
static unsigned int rand_interval(unsigned int min, unsigned int max)
{
    int r;
    const unsigned int range = 1 + max - min;
    const unsigned int buckets = RAND_MAX / range;
    const unsigned int limit = buckets * range;

    /* Create equal size buckets all in a row, then fire randomly towards
     * the buckets until you land in one of them. All buckets are equally
     * likely. If you land off the end of the line of buckets, try again. */
    do
    {
        r = rand();
    } while (r >= limit);

    return min + (r / buckets);
}

// Generate random color
//-----------------------------
static color_t random_color() {

	color_t color;
	color.r  = (uint8_t)rand_interval(8,252);
	color.g  = (uint8_t)rand_interval(8,252);
	color.b  = (uint8_t)rand_interval(8,252);
	return color;
}

//---------------------
static void _dispTime()
{
	Font curr_font = cfont;
    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

    time(&time_now);
	time_last = time_now;
	tm_info = localtime(&time_now);
	sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
	TFT_print(tmp_buff, CENTER, _height-TFT_getfontheight()-5);

    cfont = curr_font;
}

//---------------------------------
static void disp_header(char *info)
{
	TFT_fillScreen(TFT_BLACK);
	TFT_resetclipwin();

	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };

    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);
	TFT_fillRect(0, 0, _width-1, TFT_getfontheight()+8, _bg);
	TFT_drawRect(0, 0, _width-1, TFT_getfontheight()+8, TFT_CYAN);

	TFT_fillRect(0, _height-TFT_getfontheight()-9, _width-1, TFT_getfontheight()+8, _bg);
	TFT_drawRect(0, _height-TFT_getfontheight()-9, _width-1, TFT_getfontheight()+8, TFT_CYAN);

	TFT_print(info, CENTER, 4);
	_dispTime();

	_bg = TFT_BLACK;
	TFT_setclipwin(0,TFT_getfontheight()+9, _width-1, _height-TFT_getfontheight()-10);
}

//---------------------------------------------
static void update_header(char *hdr, char *ftr)
{
	color_t last_fg, last_bg;

	TFT_saveClipWin();
	TFT_resetclipwin();

	Font curr_font = cfont;
	last_bg = _bg;
	last_fg = _fg;
	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };
    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

	if (hdr) {
		TFT_fillRect(1, 1, _width-3, TFT_getfontheight()+6, _bg);
		TFT_print(hdr, CENTER, 4);
	}

	if (ftr) {
		TFT_fillRect(1, _height-TFT_getfontheight()-8, _width-3, TFT_getfontheight()+6, _bg);
		if (strlen(ftr) == 0) _dispTime();
		else TFT_print(ftr, CENTER, _height-TFT_getfontheight()-5);
	}

	cfont = curr_font;
	_fg = last_fg;
	_bg = last_bg;

	TFT_restoreClipWin();
}



// Image demo
//-------------------------
static void disp_images() {
    uint32_t tstart;

	disp_header("JPEG IMAGES");

	if (spiffs_is_mounted) {
		// ** Show scaled (1/8, 1/4, 1/2 size) JPG images
		TFT_jpg_image(CENTER, CENTER, 3, SPIFFS_BASE_PATH"/images/test1.jpg", NULL, 0);
		Wait(500);

		TFT_jpg_image(CENTER, CENTER, 2, SPIFFS_BASE_PATH"/images/test2.jpg", NULL, 0);
		Wait(500);

		TFT_jpg_image(CENTER, CENTER, 1, SPIFFS_BASE_PATH"/images/test4.jpg", NULL, 0);
		Wait(500);

		// ** Show full size JPG image
		tstart = clock();
		TFT_jpg_image(CENTER, CENTER, 0, SPIFFS_BASE_PATH"/images/test3.jpg", NULL, 0);
		tstart = clock() - tstart;
		if (doprint) printf("       JPG Decode time: %u ms\r\n", tstart);
		sprintf(tmp_buff, "Decode time: %u ms", tstart);
		update_header(NULL, tmp_buff);
		Wait(-GDEMO_INFO_TIME);

		// ** Show BMP image
		update_header("BMP IMAGE", "");
		for (int scale=5; scale >= 0; scale--) {
			tstart = clock();
			TFT_bmp_image(CENTER, CENTER, scale, SPIFFS_BASE_PATH"/images/tiger.bmp", NULL, 0);
			tstart = clock() - tstart;
			if (doprint) printf("    BMP time, scale: %d: %u ms\r\n", scale, tstart);
			sprintf(tmp_buff, "Decode time: %u ms", tstart);
			update_header(NULL, tmp_buff);
			Wait(-500);
		}
		Wait(-GDEMO_INFO_TIME);
	}
	else if (doprint) printf("  No file system found.\r\n");
}


//-----------------------
static void circle_demo()
{
	int x, y, r, n;

	disp_header("CIRCLE DEMO");

	uint32_t end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) r = rand_interval(2, x/2);
		else r = rand_interval(2, y/2);
		TFT_drawCircle(x,y,r,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d CIRCLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);

	update_header("FILLED CIRCLE", "");
	TFT_fillWindow(TFT_BLACK);
	end_time = clock() + GDEMO_TIME;
	n = 0;
	while ((clock() < end_time) && (Wait(0))) {
		x = rand_interval(8, dispWin.x2-8);
		y = rand_interval(8, dispWin.y2-8);
		if (x < y) r = rand_interval(2, x/2);
		else r = rand_interval(2, y/2);
		TFT_fillCircle(x,y,r,random_color());
		TFT_drawCircle(x,y,r,random_color());
		n++;
	}
	sprintf(tmp_buff, "%d CIRCLES", n);
	update_header(NULL, tmp_buff);
	Wait(-GDEMO_INFO_TIME);
}

//===============
void tft_demo() {

	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	TFT_resetclipwin();

	image_debug = 0;

    char dtype[16];
    
    switch (tft_disp_type) {
        case DISP_TYPE_ILI9341:
            sprintf(dtype, "ILI9341");
            break;
        case DISP_TYPE_ILI9488:
            sprintf(dtype, "ILI9488");
            break;
        case DISP_TYPE_ST7789V:
            sprintf(dtype, "ST7789V");
            break;
        case DISP_TYPE_ST7735:
            sprintf(dtype, "ST7735");
            break;
        case DISP_TYPE_ST7735R:
            sprintf(dtype, "ST7735R");
            break;
        case DISP_TYPE_ST7735B:
            sprintf(dtype, "ST7735B");
            break;
        default:
            sprintf(dtype, "Unknown");
    }
    
    uint8_t disp_rot = PORTRAIT;
	_demo_pass = 0;
	gray_scale = 0;
	doprint = 1;

	TFT_setRotation(disp_rot);
	disp_header("ESP32 TFT DEMO");
	TFT_setFont(COMIC24_FONT, NULL);
	int tempy = TFT_getfontheight() + 4;
	_fg = TFT_ORANGE;
	TFT_print("ESP32", CENTER, (dispWin.y2-dispWin.y1)/2 - tempy);
	TFT_setFont(UBUNTU16_FONT, NULL);
	_fg = TFT_CYAN;
	TFT_print("TFT Demo", CENTER, LASTY+tempy);
	tempy = TFT_getfontheight() + 4;
	TFT_setFont(DEFAULT_FONT, NULL);
	_fg = TFT_GREEN;
	sprintf(tmp_buff, "Read speed: %5.2f MHz", (float)max_rdclock/1000000.0);
	TFT_print(tmp_buff, CENTER, LASTY+tempy);

	Wait(4000);

	// demo
	// disp_header("Welcome to ESP32");
	// circle_demo();

	disp_images();

	while (1) {
		// do nothing
	}
}


//=============
void app_main()
{
    //test_sd_card();
    // ========  PREPARE DISPLAY INITIALIZATION  =========

    esp_err_t ret;

    // === SET GLOBAL VARIABLES ==========================

    // ===================================================
    // ==== Set display type                         =====
    //tft_disp_type = DEFAULT_DISP_TYPE;
	tft_disp_type = DISP_TYPE_ILI9341;
	//tft_disp_type = DISP_TYPE_ILI9488;
	//tft_disp_type = DISP_TYPE_ST7735B;
    // ===================================================

	// ===================================================
	// === Set display resolution if NOT using default ===
	// === DEFAULT_TFT_DISPLAY_WIDTH &                 ===
    // === DEFAULT_TFT_DISPLAY_HEIGHT                  ===
	_width = DEFAULT_TFT_DISPLAY_WIDTH;  // smaller dimension
	_height = DEFAULT_TFT_DISPLAY_HEIGHT; // larger dimension
	//_width = 128;  // smaller dimension
	//_height = 160; // larger dimension
	// ===================================================

	// ===================================================
	// ==== Set maximum spi clock for display read    ====
	//      operations, function 'find_rd_speed()'    ====
	//      can be used after display initialization  ====
	max_rdclock = 8000000;
	// ===================================================

    // ====================================================================
    // === Pins MUST be initialized before SPI interface initialization ===
    // ====================================================================
    TFT_PinsInit();

    // ====  CONFIGURE SPI DEVICES(s)  ====================================================================================

    spi_lobo_device_handle_t spi;
	
    spi_lobo_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,				// set SPI MISO pin
        .mosi_io_num=PIN_NUM_MOSI,				// set SPI MOSI pin
        .sclk_io_num=PIN_NUM_CLK,				// set SPI CLK pin
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
		.max_transfer_sz = 6*1024,
    };
    spi_lobo_device_interface_config_t devcfg={
        .clock_speed_hz=8000000,                // Initial clock out at 8 MHz
        .mode=0,                                // SPI mode 0
        .spics_io_num=-1,                       // we will use external CS pin
		.spics_ext_io_num=PIN_NUM_CS,           // external CS pin
		.flags=LB_SPI_DEVICE_HALFDUPLEX,        // ALWAYS SET  to HALF DUPLEX MODE!! for display spi
    };
    // ====================================================================================================================


    vTaskDelay(500 / portTICK_RATE_MS);
	printf("\r\n==============================\r\n");
    printf("TFT display DEMO, LoBo 11/2017\r\n");
	printf("==============================\r\n");
    printf("Pins used: miso=%d, mosi=%d, sck=%d, cs=%d\r\n", PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
	printf("==============================\r\n\r\n");

	// ==================================================================
	// ==== Initialize the SPI bus and attach the LCD to the SPI bus ====

	ret=spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
    assert(ret==ESP_OK);
	printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
	disp_spi = spi;

	// ==== Test select/deselect ====
	ret = spi_lobo_device_select(spi, 1);
    assert(ret==ESP_OK);
	ret = spi_lobo_device_deselect(spi);
    assert(ret==ESP_OK);

	printf("SPI: attached display device, speed=%u\r\n", spi_lobo_get_speed(spi));
	printf("SPI: bus uses native pins: %s\r\n", spi_lobo_uses_native_pins(spi) ? "true" : "false");

	// ================================
	// ==== Initialize the Display ====

	printf("SPI: display init...\r\n");
	TFT_display_init();
    printf("OK\r\n");
	
	// ---- Detect maximum read speed ----
	max_rdclock = find_rd_speed();
	printf("SPI: Max rd speed = %u\r\n", max_rdclock);

    // ==== Set SPI clock used for display operations ====
	spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
	printf("SPI: Changed speed to %u\r\n", spi_lobo_get_speed(spi));

    printf("\r\n---------------------\r\n");
	printf("Graphics demo started\r\n");
	printf("---------------------\r\n");

	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	gray_scale = 0;
    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
	TFT_setRotation(PORTRAIT);
	TFT_setFont(DEFAULT_FONT, NULL);
	TFT_resetclipwin();

	disp_header("File system INIT");
    _fg = TFT_CYAN;
	TFT_print("Initializing SPIFFS...", CENTER, CENTER);
    // ==== Initialize the file system ====
    printf("\r\n\n");
	vfs_spiffs_register();
    if (!spiffs_is_mounted) {
    	_fg = TFT_RED;
    	TFT_print("SPIFFS not mounted !", CENTER, LASTY+TFT_getfontheight()+2);
    }
    else {
    	_fg = TFT_GREEN;
    	TFT_print("SPIFFS Mounted.", CENTER, LASTY+TFT_getfontheight()+2);
    }
	Wait(-2000);

	//=========
    // Run demo
    //=========
	tft_demo();
}
