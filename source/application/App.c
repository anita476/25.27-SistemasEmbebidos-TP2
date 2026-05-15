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
#include "../drivers/HAL/include/FXOS.h"
#include "../drivers/MCAL/include/pisr.h"

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
static uint8_t uart_id;

static CommLedCmd_t out_cmd;
static bool can_tx_busy = false;

static int FXOSflag = 0;
sensor_t* angles;

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
	uart_id = UART_drv_instance_init(PORTNUM2PIN(PB, 16), PORTNUM2PIN(PB, 17), BAUDRATE);
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
	if(!FXOSinitflag){
		FXOS_Init();
		FXOSinitflag = 1;
	}

	while (1) {
		timer_drv_update(); /* must be called every iteration */

		can_process();

		CanFrame_t rx_frame;

		while (can_available()) {
			if (can_read(&rx_frame)) {
				UART_data_transmit(0, (uint8_t *) "RX ID: ", 7);

				const char hex[] = "0123456789ABCDEF";

				for (int shift = 8; shift >= 0; shift -= 4) {
					uint8_t nibble = (rx_frame.id >> shift) & 0x0F;
					uint8_t c = hex[nibble];
					UART_data_transmit(uart_id, &c, 1);
				}

				UART_data_transmit(uart_id, (uint8_t *) " DATA: ", 7);

				for (uint8_t i = 0; i < rx_frame.dlc; i++) {
					uint8_t hi = (rx_frame.data[i] >> 4) & 0x0F;
					uint8_t lo = rx_frame.data[i] & 0x0F;

					uint8_t msg[3];
					msg[0] = hex[hi];
					msg[1] = hex[lo];
					msg[2] = ' ';

					UART_data_transmit(0, msg, sizeof(msg));
				}

				UART_data_transmit(0, (uint8_t *) "\r\n", 2);
			}
		}

		if (timer_drv_expired(id)) {
			UART_data_transmit(uart_id, (uint8_t *) "101C-100", 9);
			timer_drv_start(id, 2000, TIM_MODE_SINGLESHOT, NULL);
			uint8_t out = 0x0U;
			// get_int_blocking(&out);
			get_int_blocking(&out);
			const char hex[] = "0123456789ABCDEF";
			uint8_t hi = (out >> 4) & 0x0F;
			uint8_t lo = out & 0x0F;
			uint8_t msg3[] = {'R', 'E', 'G', ':', ' ', hex[hi], hex[lo], '\r', '\n'};
			UART_data_transmit(uart_id, msg3, sizeof(msg3));

			if (!can_tx_busy) {
				if (can_send((const uint8_t *) "101C-100", 2, on_can_tx_done)) {
					can_tx_busy = true;
				}
			}
		}
		angles = FXOSgetAngles();
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