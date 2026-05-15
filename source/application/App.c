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

static uint8_t buf[SENSOR_ASCII_BUF_SIZE];

volatile static int FXOSflag = 0;
sensor_t *angles;

/*******************************************************************************
 * PRIVATE FUNCTION DECLARATIONS
 ******************************************************************************/
static EVENT App_CaptureEvent(void);
static void on_can_tx_done(bool success);

/* todo make them diff messages ! */
uint8_t sensor_to_ascii(const sensor_t *sensor, uint8_t *buf, uint8_t buf_size) {
	if (sensor == NULL || buf == NULL) {
		return 0u;
	}

	/* Each angle: id(1) + sign(1) + up to 3 digits + \r\n = 7 bytes max
	 * 3 angles = 21 bytes max                                            */
	if (buf_size < 21u) {
		return 0u;
	}

	const struct {
		char id;
		angle_t val;
	} angles[3] = {
		{'R', sensor->roll},
		{'C', sensor->pitch},
		{'O', sensor->yaw},
	};

	uint8_t pos = 0u;

	for (uint8_t i = 0u; i < 3u; i++) {
		angle_t val = angles[i].val;

		/* ID character */
		buf[pos++] = (uint8_t) angles[i].id;

		/* Sign */
		if (val < 0) {
			buf[pos++] = '-';
			val = -val;
		} else {
			buf[pos++] = '+';
		}

		if (val >= 100) {
			buf[pos++] = (uint8_t) ('0' + (val / 100) % 10);
		}
		if (val >= 10) {
			buf[pos++] = (uint8_t) ('0' + (val / 10) % 10);
		}
		buf[pos++] = (uint8_t) ('0' + (val % 10));

		buf[pos++] = '\r';
		buf[pos++] = '\n';
	}

	return pos;
}
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
				// process_can_frame(rx_frame);
			}
		}

		if (timer_drv_expired(id)) {
			timer_drv_start(id, 2000, TIM_MODE_SINGLESHOT, NULL);
			uint8_t len = sensor_to_ascii(angles, buf, sizeof(buf));
			UART_data_transmit(uart_id, (unsigned char *) buf, len);
			if (!can_tx_busy) {
				if (can_send((const uint8_t *) "101C-100", 2, on_can_tx_done)) {
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