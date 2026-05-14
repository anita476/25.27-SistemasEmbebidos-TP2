/***************************************************************************/ /**
   @file     App.c
   @brief    Application functions
   @author   Nicolás Magliola
  ******************************************************************************/
/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/
#include "../drivers/HAL/include/board_led.h"
#include "../drivers/HAL/include/can_controller.h"
#include "../drivers/HAL/include/communication.h"
#include "../drivers/HAL/include/switch.h"
#include "../drivers/HAL/include/timer.h"
#include "../drivers/MCAL/include/uart.h"
#include "include/App_commons.h"
#include "include/fsm_table.h"
#include "tests/include/spi_test.h"
#include "tests/include/uart_test.h"

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/
/**
 * global variable, will be used by fsm
 * since we are working sequentially and interrupts dont access it or use it, it should be safe
 */
AppContext_t g_app_ctx = {
	.current_state = NULL,
};

static uint32_t id;
static CommLedCmd_t out_cmd;
static bool can_tx_busy = false;

/*******************************************************************************
 * PRIVATE FUNCTION DECLARATIONS
 ******************************************************************************/
static EVENT App_CaptureEvent(void);
static void on_can_tx_done(bool success);

/*******************************************************************************
 * GLOBAL FUNCTION DEFINITIONS
 ******************************************************************************/

/* interrupts are disabled at this point */
void App_Init(void) {
	timer_drv_init();
	board_led_drv_init();

	id = timer_drv_get_id();
	timer_drv_start(id, 2000, TIM_MODE_SINGLESHOT, NULL);

	g_app_ctx.current_state = FSM_GetInitState();
}

void App_Run(void) {
	board_led_drv_state(GREEN, true);
	board_led_drv_state(BLUE, true);

	if (!can_controller_drv_init()) {
		board_led_drv_state(RED, true);
		while (1) {
		}
	}

	while (1) {
		timer_drv_update(); /* must be called every iteration */

		can_process();

		if (timer_drv_expired(id)) {
			UART_data_transmit(0, (uint8_t *) "2 seconds\r\n", 12);
			timer_drv_start(id, 2000, TIM_MODE_SINGLESHOT, NULL);

			if (!can_tx_busy) {
				if (can_send((const uint8_t *) "A", 2, on_can_tx_done)) {
					can_tx_busy = true;
				}
			}
		}
	}
}

/*******************************************************************************
 * PRIVATE FUNCTION DEFINITIONS
 ******************************************************************************/

/**
 * @brief Called from can_process() when a TX completes or fails.
 */
static void on_can_tx_done(bool success) {
	can_tx_busy = false;
	if (!success) {
		/* @todo handle TX error — e.g. retry, log, set error LED */
	}
}

static EVENT App_CaptureEvent(void) {
	return EV_NONE;
}