/***************************************************************************/ /**
   @file     App.c
   @brief    Application functions
   @author   Nicolás Magliola
  ******************************************************************************/
/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/
#include "../drivers/HAL/include/FXOS.h"
#include "../drivers/HAL/include/board_led.h"
#include "../drivers/HAL/include/can_comm.h"
#include "../drivers/HAL/include/can_controller.h"
#include "../drivers/HAL/include/communication.h"
#include "../drivers/HAL/include/switch.h"
#include "../drivers/HAL/include/timer.h"
#include "../drivers/MCAL/include/pisr.h"
#include "../drivers/MCAL/include/uart.h"
#include "include/App_commons.h"
#include "include/fsm_table.h"
#include "tests/include/spi_test.h"
#include "tests/include/uart_test.h"

#define SENSOR_ASCII_BUF_SIZE 24U

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
static uint8_t angle_turn = 0u;

static uint8_t buf[SENSOR_ASCII_BUF_SIZE];

volatile static int FXOSflag = 0;
sensor_t *angles;
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
	if (!communication_drv_init()) {
		board_led_drv_state(RED, true);
	}
	id = timer_drv_get_id();
	timer_drv_start(id, 2000, TIM_MODE_SINGLESHOT, NULL);
	g_app_ctx.current_state = FSM_GetInitState();
}

void App_Run(void) {
	/* INITIALIZE CAN CONTROLLER */
	if (!can_controller_drv_init()) {
		board_led_drv_state(RED, true);
		while (1) {
			; /* do nothing til death*/
		}
	} else {
		board_led_drv_state(GREEN, true);
	}
	if (!FXOSflag) {
		FXOS_Init();
		FXOSflag = 1;
	}

	while (1) {
		timer_drv_update(); /* must be called every iteration */
		can_process();		/* must be called every iteration */
		angles = FXOSgetAngles();
		CanFrame_t rx_frame;

		/* process all available can franes */
		while (can_available()) {
			if (can_read(&rx_frame)) {
				process_can_frame(rx_frame);
			}
		}

		if (timer_drv_expired(id)) {
			timer_drv_start(id, 2000, TIM_MODE_SINGLESHOT, NULL);

			communication_drv_send_angle('C', angles->pitch);
			communication_drv_send_angle('R', angles->roll);
			communication_drv_send_angle('O', angles->yaw);

			if (!can_tx_busy) {
				bool sent = false;
				switch (angle_turn) {
					case 0u:
						sent = can_send_angle(angles->roll, 'R', on_can_tx_done);
						break;
					case 1u:
						sent = can_send_angle(angles->pitch, 'C', on_can_tx_done);
						break;
					case 2u:
						sent = can_send_angle(angles->yaw, 'O', on_can_tx_done);
						break;
					default:
						angle_turn = 0u;
						break;
				}
				if (sent) {
					can_tx_busy = true;
					angle_turn = (angle_turn + 1u) % 3u;
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
