/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>
#include <string.h>
#include "ili9341.h"
#include "lvgl.h"
#include "touch/touch.h"

LV_FONT_DECLARE(dseg70);
LV_FONT_DECLARE(dseg50);
LV_FONT_DECLARE(dseg40);
LV_FONT_DECLARE(dseg30);
LV_FONT_DECLARE(clock);

#define SYMBOL_CLOCK "\xEF\x80\x97"

/************************************************************************/
/* LCD / LVGL                                                           */
/************************************************************************/

#define VERTICAL_ORIENTATION /* Comment this line to set horizontal (CTRL+K+C) */
#ifdef VERTICAL_ORIENTATION
#	define LV_HOR_RES_MAX          (240)
#	define LV_VER_RES_MAX          (320)
#else
#	define LV_HOR_RES_MAX          (320)
#	define LV_VER_RES_MAX          (240)
#endif

/*A static or global variable to store the buffers*/
static lv_disp_draw_buf_t disp_buf;

/*Static or global buffer(s). The second buffer is optional*/
static lv_color_t buf_1[LV_HOR_RES_MAX * LV_VER_RES_MAX];
static lv_disp_drv_t disp_drv;          /*A variable to hold the drivers. Must be static or global.*/
static lv_indev_drv_t indev_drv;

static lv_obj_t *labelSetValue;
static lv_obj_t *labelClock;

typedef struct  {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t week;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
} calendar;

/************************************************************************/
/* RTOS                                                                 */
/************************************************************************/

#define TASK_LCD_STACK_SIZE					(1024*6/sizeof(portSTACK_TYPE))
#define TASK_LCD_STACK_PRIORITY				(tskIDLE_PRIORITY)
#define TASK_RTC_STACK_SIZE					(1024*6/sizeof(portSTACK_TYPE))
#define TASK_RTC_STACK_PRIORITY				(tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {	}
}

extern void vApplicationIdleHook(void) { }

extern void vApplicationTickHook(void) { }

extern void vApplicationMallocFailedHook(void) {
	configASSERT( ( volatile void * ) NULL );
}

SemaphoreHandle_t xSemaphoreRTC;

/************************************************************************/
/* PROTOTYPES                                                           */
/************************************************************************/
void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type);

/************************************************************************/
/* lvgl                                                                 */
/************************************************************************/

/**
* \brief Interrupt handler for the RTC. Refresh the display.
*/
void RTC_Handler(void) {
	uint32_t ul_status = rtc_get_status(RTC);
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	
	/* seccond tick */
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		// o código para irq de segundo vem aqui
		xSemaphoreGiveFromISR(xSemaphoreRTC, 0);
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		// o código para irq de alame vem aqui
	}

	rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}

static void power_handler(lv_event_t * e) {
	LV_LOG_USER("Clicked power");
}

static void menu_handler(lv_event_t * e) {
	LV_LOG_USER("Clicked menu");
}

static void clk_handler(lv_event_t * e) {
	LV_LOG_USER("Clicked clock");
}

static void home_handler(lv_event_t * e) {
	LV_LOG_USER("Clicked home");
}

static void down_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	if (code == LV_EVENT_CLICKED) {
		c = lv_label_get_text(labelSetValue);
		temp = atoi(c);
		lv_label_set_text_fmt(labelSetValue, "%02d", temp - 1);
	}
}

static void up_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	if (code == LV_EVENT_CLICKED) {
		c = lv_label_get_text(labelSetValue);
		temp = atoi(c);
		lv_label_set_text_fmt(labelSetValue, "%02d", temp + 1);
	}
}

// termostato
void lv_termostato(void) {
	float floor_temperature = 23.4;
	
	static lv_style_t style;
	lv_style_init(&style);
	lv_style_set_bg_color(&style, lv_color_black());
	
	// ----- POWER -----
	lv_obj_t *labelLeftBracket1 = lv_label_create(lv_scr_act());
	lv_obj_align(labelLeftBracket1, LV_ALIGN_BOTTOM_LEFT, 4, -20);
	lv_obj_set_style_text_color(labelLeftBracket1, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelLeftBracket1, "[  ");
	
	lv_obj_t *btnPower = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btnPower, power_handler, LV_EVENT_ALL, NULL);
	lv_obj_align_to(btnPower, labelLeftBracket1, LV_ALIGN_OUT_RIGHT_MID, -4, -8);
	lv_obj_add_style(btnPower, &style, 0);
	
	lv_obj_t *labelPower = lv_label_create(btnPower);
	lv_label_set_text(labelPower, LV_SYMBOL_POWER);
	lv_obj_center(labelPower);
	
	// ----- MENU -----
	lv_obj_t *labelLeftPipe = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelLeftPipe, labelPower, LV_ALIGN_OUT_RIGHT_MID, 0, -2);
	lv_obj_set_style_text_color(labelLeftPipe, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelLeftPipe, "  |  ");
	
	lv_obj_t *btnMenu = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btnMenu, menu_handler, LV_EVENT_ALL, NULL);
	lv_obj_align_to(btnMenu, labelLeftPipe, LV_ALIGN_OUT_RIGHT_MID, -6, -6);
	lv_obj_add_style(btnMenu, &style, 0);
	
	lv_obj_t *labelMenu = lv_label_create(btnMenu);
	lv_label_set_text(labelMenu, "M");
	lv_obj_center(labelMenu);
	
	lv_obj_t *labelRightPipe = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelRightPipe, labelMenu, LV_ALIGN_OUT_RIGHT_MID, 0, -3);
	lv_obj_set_style_text_color(labelRightPipe, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelRightPipe, "  |  ");
	
	// ----- CLOCK -----
	lv_obj_t *btnClk = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btnClk, clk_handler, LV_EVENT_ALL, NULL);
	lv_obj_align_to(btnClk, labelRightPipe, LV_ALIGN_OUT_RIGHT_MID, -2, -5);
	lv_obj_add_style(btnClk, &style, 0);
	
	lv_obj_t *labelClk = lv_label_create(btnClk);
	lv_obj_set_style_text_font(labelClk, &clock, LV_STATE_DEFAULT);
	lv_label_set_text(labelClk, SYMBOL_CLOCK);
	lv_obj_center(labelClk);
	
	lv_obj_t *labelRightBracket1 = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelRightBracket1, labelClk, LV_ALIGN_OUT_RIGHT_MID, 0, -1);
	lv_obj_set_style_text_color(labelRightBracket1, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelRightBracket1, "  ]");
	
	// ----- HOME -----
	lv_obj_t *btnHome = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btnHome, home_handler, LV_EVENT_ALL, NULL);
	lv_obj_align(btnHome, LV_ALIGN_CENTER, 30, 40);
	lv_obj_add_style(btnHome, &style, 0);
	
	lv_obj_t *labelHome = lv_label_create(btnHome);
	lv_label_set_text(labelHome, LV_SYMBOL_HOME);
	lv_obj_center(labelHome);
	
	// ----- DOWN -----
	lv_obj_t *labelRightBracket2 = lv_label_create(lv_scr_act());
	lv_obj_align(labelRightBracket2, LV_ALIGN_BOTTOM_RIGHT, -4, -20);
	lv_obj_set_style_text_color(labelRightBracket2, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelRightBracket2, "  ]");
	
	lv_obj_t *btnDown = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btnDown, down_handler, LV_EVENT_ALL, NULL);
	lv_obj_align_to(btnDown, labelRightBracket2, LV_ALIGN_OUT_LEFT_MID, 0, -4);
	lv_obj_add_style(btnDown, &style, 0);
	
	lv_obj_t *labelDown = lv_label_create(btnDown);
	lv_label_set_text(labelDown, LV_SYMBOL_DOWN);
	lv_obj_center(labelDown);
	
	lv_obj_t *labelArrowPipe = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelArrowPipe, labelDown, LV_ALIGN_OUT_LEFT_MID, 16, -5);
	lv_obj_set_style_text_color(labelArrowPipe, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelArrowPipe, "  |  ");
	
	// ----- UP -----
	lv_obj_t *btnUp = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btnUp, up_handler, LV_EVENT_ALL, NULL);
	lv_obj_align_to(btnUp, labelArrowPipe, LV_ALIGN_OUT_LEFT_MID, 0, -5);
	lv_obj_add_style(btnUp, &style, 0);
	
	lv_obj_t *labelUp = lv_label_create(btnUp);
	lv_label_set_text(labelUp, LV_SYMBOL_UP);
	lv_obj_center(labelUp);
	
	lv_obj_t *labelLeftBracket2 = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelLeftBracket2, labelUp, LV_ALIGN_OUT_LEFT_MID, 30, -4);
	lv_obj_set_style_text_color(labelLeftBracket2, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelLeftBracket2, "[");
	
	// ----- FLOOR -----
	lv_obj_t *labelFloor = lv_label_create(lv_scr_act());
	lv_obj_align(labelFloor, LV_ALIGN_LEFT_MID, 20 , -25);
	lv_obj_set_style_text_font(labelFloor, &dseg70, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelFloor, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelFloor, "%02d.", (int) floor_temperature);
	
	// ----- FLOOR DECIMAL -----
	lv_obj_t *labelFloorDecimal = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelFloorDecimal, labelFloor, LV_ALIGN_OUT_RIGHT_MID, 4, 8);
	lv_obj_set_style_text_font(labelFloorDecimal, &dseg40, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelFloorDecimal, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelFloorDecimal, "%01d", (int) (floor_temperature * 10) % 10);
	
	// ----- CLOCK -----
	labelClock = lv_label_create(lv_scr_act());
	lv_obj_align(labelClock, LV_ALIGN_TOP_RIGHT, -10 , 10);
	lv_obj_set_style_text_font(labelClock, &dseg30, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelClock, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelClock, "00:00");
	
	// ----- SET VALUE -----
	labelSetValue = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelSetValue, labelClock, LV_ALIGN_OUT_BOTTOM_MID, -15 , 20);
	lv_obj_set_style_text_font(labelSetValue, &dseg50, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelSetValue, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelSetValue, "%02d", 22);
	
	// ----- CELSIUS -----
	lv_obj_t *labelCelsius1 = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelCelsius1, labelSetValue, LV_ALIGN_OUT_RIGHT_MID, 5, -20);
	lv_obj_set_style_text_color(labelCelsius1, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelCelsius1, "c");
	
	lv_obj_t *labelCelsius2 = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelCelsius2, labelFloor, LV_ALIGN_OUT_RIGHT_MID, 10, -30);
	lv_obj_set_style_text_color(labelCelsius2, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelCelsius2, "c");
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_lcd(void *pvParameters) {
	int px, py;

	lv_termostato();

	for (;;)  {
		lv_tick_inc(50);
		lv_task_handler();
		vTaskDelay(50);
	}
}

static void task_rtc(void *pvParameters) {
	calendar now = {
		.year = 2023,
		.month = 05,
		.day = 01,
		.week = 1,
		.hour = 17,
		.minute = 00,
		.second = 0,
	};

	RTC_init(RTC, ID_RTC, now, RTC_IER_SECEN);

	for(;;) {
		if(xSemaphoreTake(xSemaphoreRTC, 0)) {
			/* Leitura do valor atual do RTC */
			rtc_get_time(RTC, &now.hour, &now.minute, &now.second);
			rtc_get_date(RTC, &now.year, &now.month, &now.day, &now.week);

			/* Atualização do valor do clock */
			if (now.second % 2 == 0) {
				lv_label_set_text_fmt(labelClock, "%02d:%02d", now.hour, now.minute);
			}
			else {
				lv_label_set_text_fmt(labelClock, "%02d %02d", now.hour, now.minute);
			}
		}
		vTaskDelay(700);
	}
}

/************************************************************************/
/* configs                                                              */
/************************************************************************/

static void configure_lcd(void) {
	/**LCD pin configure on SPI*/
	pio_configure_pin(LCD_SPI_MISO_PIO, LCD_SPI_MISO_FLAGS);  //
	pio_configure_pin(LCD_SPI_MOSI_PIO, LCD_SPI_MOSI_FLAGS);
	pio_configure_pin(LCD_SPI_SPCK_PIO, LCD_SPI_SPCK_FLAGS);
	pio_configure_pin(LCD_SPI_NPCS_PIO, LCD_SPI_NPCS_FLAGS);
	pio_configure_pin(LCD_SPI_RESET_PIO, LCD_SPI_RESET_FLAGS);
	pio_configure_pin(LCD_SPI_CDS_PIO, LCD_SPI_CDS_FLAGS);
	
	ili9341_init();
	ili9341_backlight_on();
}

static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = USART_SERIAL_EXAMPLE_BAUDRATE,
		.charlength = USART_SERIAL_CHAR_LENGTH,
		.paritytype = USART_SERIAL_PARITY,
		.stopbits = USART_SERIAL_STOP_BIT,
	};

	/* Configure console UART. */
	stdio_serial_init(CONSOLE_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}

/**
* Configura o RTC para funcionar com interrupcao de alarme
*/
void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type) {
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(rtc, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(rtc, t.year, t.month, t.day, t.week);
	rtc_set_time(rtc, t.hour, t.minute, t.second);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(id_rtc);
	NVIC_ClearPendingIRQ(id_rtc);
	NVIC_SetPriority(id_rtc, 4);
	NVIC_EnableIRQ(id_rtc);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(rtc,  irq_type);
}

/************************************************************************/
/* port lvgl                                                            */
/************************************************************************/

void my_flush_cb(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
	ili9341_set_top_left_limit(area->x1, area->y1);   ili9341_set_bottom_right_limit(area->x2, area->y2);
	ili9341_copy_pixels_to_screen(color_p,  (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
	
	/* IMPORTANT!!!
	* Inform the graphics library that you are ready with the flushing*/
	lv_disp_flush_ready(disp_drv);
}

void my_input_read(lv_indev_drv_t * drv, lv_indev_data_t*data) {
	int px, py, pressed;
	
	if (readPoint(&px, &py))
		data->state = LV_INDEV_STATE_PRESSED;
	else
		data->state = LV_INDEV_STATE_RELEASED; 
	
	#ifdef VERTICAL_ORIENTATION
	data->point.x = py;
	data->point.y = 320 - px;
	#else
	data->point.x = px;
	data->point.y = py;
	#endif
}

void configure_lvgl(void) {
	lv_init();
	lv_disp_draw_buf_init(&disp_buf, buf_1, NULL, LV_HOR_RES_MAX * LV_VER_RES_MAX);
	
	lv_disp_drv_init(&disp_drv);            /*Basic initialization*/
	disp_drv.draw_buf = &disp_buf;          /*Set an initialized buffer*/
	disp_drv.flush_cb = my_flush_cb;        /*Set a flush callback to draw to the display*/
	disp_drv.hor_res = LV_HOR_RES_MAX;      /*Set the horizontal resolution in pixels*/
	disp_drv.ver_res = LV_VER_RES_MAX;      /*Set the vertical resolution in pixels*/

	lv_disp_t * disp;
	disp = lv_disp_drv_register(&disp_drv); /*Register the driver and save the created display objects*/
	
	/* Init input on LVGL */
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = my_input_read;
	lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/
int main(void) {
	/* board and sys init */
	board_init();
	sysclk_init();
	configure_console();

	/* LCd, touch and lvgl init*/
	configure_lcd();
	#ifdef VERTICAL_ORIENTATION
	ili9341_set_orientation(ILI9341_FLIP_Y | ILI9341_SWITCH_XY);
	#endif
	configure_touch();
	configure_lvgl();
	
	/* Attempt to create a semaphore. */
	xSemaphoreRTC = xSemaphoreCreateBinary();
	if (xSemaphoreRTC == NULL) {
		printf("Failed to create RTC semaphore\n");
	}

	/* Create task to control LCD */
	if (xTaskCreate(task_lcd, "LCD", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create lcd task\r\n");
	}
	
	/* Create task to read RTC */
	if (xTaskCreate(task_rtc, "RTC", TASK_RTC_STACK_SIZE, NULL, TASK_RTC_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create RTC task\r\n");
	}
	
	/* Start the scheduler. */
	vTaskStartScheduler();

	while(1){ }
}
