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
 * */
AppContext_t g_app_ctx = {
	.current_state = NULL, // set after initing tabl
};

static uint32_t id;
static CommLedCmd_t out_cmd;
/*******************************************************************************
 *******************************************************************************
						GLOBAL FUNCTION DEFINITIONS
*******************************************************************************/

static EVENT App_CaptureEvent();
/********************************************************************************
******************************************************************************/
/* interrupts are disabled at this point*/
void App_Init(void) {
	// timer_drv_init();
	//   FSM_InitTable();
	board_led_drv_init();
	// communication_drv_init();
	// id = UART_drv_instance_init(PORTNUM2PIN(PB, 16), PORTNUM2PIN(PB, 17), BAUDRATE);
	//   initial state
	// id = timer_drv_get_id();
	// timer_drv_start(id, 2000, TIM_MODE_PERIODIC, NULL);
	g_app_ctx.current_state = FSM_GetInitState();
}

/* Función que se llama constantemente en un ciclo infinito */
void App_Run(void) {
	// spi_test_app(id);
	board_led_drv_state(GREEN, true);
	// board_led_drv_state(RED, true);
	board_led_drv_state(BLUE, true);
	// communication_drv_send_angle_ascii(COMM_ANGLE_ORIENTATION, "-134", 4);

	can_controller_drv_init();

	while (1) {
		// if (communication_drv_receive_led_cmd(&out_cmd)) {
		//  @todo should probably have a better driver !!
		//	if (out_cmd.group == CURRENT_GROUP_ID) {
		//		board_led_drv_state(RED, out_cmd.red);
		//		board_led_drv_state(GREEN, out_cmd.green);
		//		board_led_drv_state(BLUE, out_cmd.blue);
		//	}
		//}
		// timer_drv_update(); /* must be called every iteration */
		// if (timer_drv_expired(id)) {
		// send data to can
		//}
		// communication_drv_send_angle_ascii(COMM_ANGLE_ORIENTATION, "+13", 3);
		//  bool res = uart_test(id);
		//  if (res) {
		//	printf("Uart test completed successfully\n");
		//  }
		;
		// EVENT curr_event = App_CaptureEvent();

		// Feed event to FSM if theres something
		// if (curr_event != EV_NONE) {
		//	g_app_ctx.current_state = fsm(g_app_ctx.current_state, curr_event);
		//}
	}
}

static EVENT App_CaptureEvent() {
	/**
	 * Capture events as they appear
	 **/
	return EV_NONE;
}
