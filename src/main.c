/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>
#include <string.h>
#include "ili9341.h"
#include "lvgl.h"
#include "touch/touch.h"

LV_FONT_DECLARE(dseg40);

#define SYMBOL_CLOCK "\xEF\x80\x97"

#define SPD_PIO			PIOA
#define SPD_PIO_ID		ID_PIOA
#define SPD_IDX			19
#define SPD_IDX_MASK	(1 << SPD_IDX)

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

static lv_obj_t *labelClock;
static lv_obj_t *labelSpeed;
static lv_obj_t *labelXLR8;

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
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource);

/************************************************************************/
/* callbacks                                                            */
/************************************************************************/

void RTT_Handler(void) {
	uint32_t ul_status;
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		
	}
}

/**
* \brief Interrupt handler for the RTC. Refresh the display.
*/
void RTC_Handler(void) {
	uint32_t ul_status = rtc_get_status(RTC);
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	
	/* seccond tick */
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		// o c�digo para irq de segundo vem aqui
		xSemaphoreGiveFromISR(xSemaphoreRTC, 0);
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		// o c�digo para irq de alame vem aqui
	}

	rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}

void callback_spd(void) {
	
}

/************************************************************************/
/* lvgl                                                                 */
/************************************************************************/

// bike
void lv_bike(void) {
	// ----- CLOCK -----
	labelClock = lv_label_create(lv_scr_act());
	lv_obj_align(labelClock, LV_ALIGN_TOP_MID, 0 , 50);
	lv_obj_set_style_text_font(labelClock, &dseg40, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelClock, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelClock, "00:00:00");
	
	// ----- CLOCK TITLE -----
	lv_label_t *labelClockTitle = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelClockTitle, labelClock, LV_ALIGN_OUT_TOP_LEFT, 0 , -10);
	lv_obj_set_style_text_color(labelClockTitle, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelClockTitle, "CLOCK");
	
	// ----- HORIZONTAL LINE 1 -----
	lv_label_t *labelHLine1 = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelHLine1, labelClock, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
	lv_obj_set_style_text_color(labelHLine1, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelHLine1, "..........................................");
	
	// ----- SPEED TITLE -----
	lv_label_t *labelSpeedTitle = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelSpeedTitle, labelHLine1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
	lv_obj_set_style_text_color(labelSpeedTitle, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelSpeedTitle, "SPEED   KM/H");
	
	// ----- SPEED -----
	labelSpeed = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelSpeed, labelSpeedTitle, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
	lv_obj_set_style_text_font(labelSpeed, &dseg40, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelSpeed, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelSpeed, "25");
	
	// ----- HORIZONTAL LINE 2 -----
	lv_label_t *labelHLine2 = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelHLine2, labelSpeed, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
	lv_obj_set_style_text_color(labelHLine2, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelHLine2, "..........................................");
	
	// ----- XLR8 TITLE -----
	lv_label_t *labelXLR8Title = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelXLR8Title, labelHLine2, LV_ALIGN_OUT_BOTTOM_LEFT, 0 , 10);
	lv_obj_set_style_text_color(labelXLR8Title, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelXLR8Title, "ACC");
	
	// ----- XLR8 -----
	labelXLR8 = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelXLR8, labelXLR8Title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
	lv_obj_set_style_text_color(labelXLR8, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelXLR8, LV_SYMBOL_UP); /* If acc is positive */
	//lv_label_set_text_fmt(labelXLR8, LV_SYMBOL_MINUS); /* If acc is aprox zero */
	//lv_label_set_text_fmt(labelXLR8, LV_SYMBOL_DOWN); /* If acc is negative */
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_lcd(void *pvParameters) {
	int px, py;

	lv_bike();

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
		.hour = 22,
		.minute = 22,
		.second = 22,
	};

	RTC_init(RTC, ID_RTC, now, RTC_IER_SECEN);

	for(;;) {
		if(xSemaphoreTake(xSemaphoreRTC, 0)) {
			/* Leitura do valor atual do RTC */
			rtc_get_time(RTC, &now.hour, &now.minute, &now.second);
			rtc_get_date(RTC, &now.year, &now.month, &now.day, &now.week);

			/* Atualiza��o do valor do clock */
			lv_label_set_text_fmt(labelClock, "%02d:%02d:%02d", now.hour, now.minute, now.second);
		}
		vTaskDelay(700);
	}
}

static void task_spd(void *pvParameters) {

	pmc_enable_periph_clk(ID_PIOA);
	pio_configure(PIOA, PIO_INPUT, SPD_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_handler_set(PIOA, ID_PIOA, SPD_IDX_MASK, PIO_IT_FALL_EDGE, callback_spd);
	pio_enable_interrupt(PIOA, SPD_IDX_MASK);
	pio_get_interrupt_status(PIOA);
	NVIC_EnableIRQ(ID_PIOA);
	NVIC_SetPriority(ID_PIOA, 4);

	RTT_init(1000, 6000, RTT_MR_ALMIEN);

	for (;;) {
		
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

static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource) {

	uint16_t pllPreScale = (int) (((float) 32768) / freqPrescale);

	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);

	if (rttIRQSource & RTT_MR_ALMIEN) {
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT));
		rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);
	}

	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN)) {
		rtt_enable_interrupt(RTT, rttIRQSource);
	}
	else {
		rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
	}
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

	/* Create task to read SPD */
	if (xTaskCreate(task_spd, "SPD", TASK_RTC_STACK_SIZE, NULL, TASK_RTC_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create SPD task\r\n");
	}
	
	/* Start the scheduler. */
	vTaskStartScheduler();

	while(1){ }
}
