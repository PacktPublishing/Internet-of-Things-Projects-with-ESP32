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
#include <driver/adc.h>

#include <math.h>
// ==========================================================
// Define which spi bus to use TFT_VSPI_HOST or TFT_HSPI_HOST
#define SPI_BUS TFT_HSPI_HOST
// ==========================================================

#define BUZZER 27

typedef struct {
	int 	x;
	int		y;
	int		r;
	int 	deleted;
	color_t     color;
} Circle_Sprite;

static int _demo_pass = 0;
static uint8_t doprint = 1;
static struct tm* tm_info;
static char tmp_buff[64];
static time_t time_now, time_last = 0;

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

long map_to_scren(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int check_insideCircle(int x, int y, Circle_Sprite sp){
	int d = sqrt(pow(x-sp.x,2)+pow(y-sp.y,2));

	if(d<=sp.r)
		return 1;
	else{
		int rr = d - sp.r - 8;
		if(rr<=0)
			return 1;
		else
			return 0;
	}
		
}

//-----------------------
static void game_demo()
{
	int x, y, r, i, n;
	n = 10;
	Circle_Sprite circles[n];

	// initialize ADC
	adc1_config_width(ADC_WIDTH_12Bit);
   	adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_11db);	
   	adc2_config_channel_atten(ADC2_CHANNEL_3, ADC_ATTEN_11db);	 

	// set gpio and its direction for buzzer
    gpio_pad_select_gpio(BUZZER);    
    gpio_set_direction(BUZZER, GPIO_MODE_OUTPUT);    

	// initialize screen
	TFT_resetclipwin();
	disp_header("Circle Game Demo");
	update_header(NULL, "Move your joystick to circle");	

	// generate circles
	
	for(i=0;i<n;i++){
		x = rand_interval(16, dispWin.x2-16);
		y = rand_interval(32, dispWin.y2-32);

		r = rand_interval(8, 16);
		color_t c = random_color();
		TFT_fillCircle(x,y,r,c);
		circles[i].x = x;
		circles[i].y = y;
		circles[i].r = r;
		circles[i].deleted = 0;
		circles[i].color = c;
	}
	
	int joyX, joyY;
	int running = 1;
	r = 8;
	int curr_x = 100, curr_y = 100;
	int cx = 151, cy = 212;

	TFT_fillCircle(curr_x,curr_y,r,TFT_RED);
	vTaskDelay(1000 / portTICK_PERIOD_MS);	
	TFT_drawCircle(curr_x,curr_y,r,TFT_BLACK);

	char tmp_buff[64];
	int sound = 0;
	while(running){		

		joyX = adc1_get_raw(ADC1_CHANNEL_7);				
		adc2_get_raw(ADC2_CHANNEL_3,ADC_WIDTH_12Bit, &joyY);
		

		joyX = map_to_scren(joyX,0,4095,0,dispWin.x2);
		joyY = map_to_scren(joyY,0,4095,0,dispWin.y2);
		
		
		// validate point to screen
		curr_x = curr_x + joyX - cx;
		curr_y = curr_y + joyY - cy;		

		if(curr_x<8)
			curr_x = 8;
		if(curr_x>(dispWin.x2-8))
			curr_x = dispWin.x2 - 8;
		if(curr_y<32)
			curr_y = 32;
		if(curr_y>(dispWin.y2-32))
			curr_y = dispWin.y2-32;

		// check a ball inside a circle
		for(i=0;i<n;i++){
			if(circles[i].deleted == 1)
				continue;
			if(check_insideCircle(curr_x,curr_y,circles[i])==1){
				gpio_set_level(BUZZER, 1);
				sound = 1;
				circles[i].deleted = 1;
				TFT_fillCircle(circles[i].x,circles[i].y,circles[i].r,TFT_BLACK);	
				break;	
			}
		} 	
		TFT_fillCircle(curr_x,curr_y,r,TFT_RED);

		_fg = TFT_YELLOW;
		sprintf(tmp_buff, "x: %d y: %d             ", curr_x,curr_y);
		TFT_print(tmp_buff, 4, 4);
						
		vTaskDelay(200 / portTICK_PERIOD_MS);		
		TFT_fillCircle(curr_x,curr_y,r,TFT_BLACK);
		_fg = TFT_BLACK;		
		TFT_print(tmp_buff, 4, 4);
		if(sound==1){
			gpio_set_level(BUZZER, 0);
			sound = 0;
		}

		// check if finished
		int nn = 0;
		for(i=0;i<n;i++){
			if(circles[i].deleted == 1)
				nn++;				
		}
		if(nn==n)
			break;
		
	}
	TFT_resetclipwin();
	disp_header("ESP32 Game DEMO");
	TFT_setFont(COMIC24_FONT, NULL);
	int tempy = TFT_getfontheight() + 4;
	_fg = TFT_ORANGE;
	TFT_print("ESP32-", CENTER, (dispWin.y2-dispWin.y1)/2 - tempy);
	TFT_setFont(UBUNTU16_FONT, NULL);
	_fg = TFT_CYAN;
	TFT_print("Game Over", CENTER, LASTY+tempy);
	tempy = TFT_getfontheight() + 4;
	TFT_setFont(DEFAULT_FONT, NULL);
	
	while(1){

	}
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
	disp_header("ESP32 Game DEMO");
	TFT_setFont(COMIC24_FONT, NULL);
	int tempy = TFT_getfontheight() + 4;
	_fg = TFT_ORANGE;
	TFT_print("ESP32", CENTER, (dispWin.y2-dispWin.y1)/2 - tempy);
	TFT_setFont(UBUNTU16_FONT, NULL);
	_fg = TFT_CYAN;
	TFT_print("Game Demo", CENTER, LASTY+tempy);
	tempy = TFT_getfontheight() + 4;
	TFT_setFont(DEFAULT_FONT, NULL);
	_fg = TFT_GREEN;
	sprintf(tmp_buff, "Read speed: %5.2f MHz", (float)max_rdclock/1000000.0);
	TFT_print(tmp_buff, CENTER, LASTY+tempy);

	Wait(4000);

	game_demo();

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


	//=========
    // Run demo
    //=========
	tft_demo();
}
