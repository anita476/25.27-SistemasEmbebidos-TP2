/***************************************************************************/ /**
   @file     App.c
   @brief    Application functions
   @author   Nicolás Magliola
  ******************************************************************************/

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "../drivers/HAL/include/switch.h"
#include "../drivers/HAL/include/timer.h"
#include "../drivers/MCAL/include/UART.h"

#include "include/App_commons.h"
#include "include/fsm_table.h"
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

static const char MSG[] = "The quick brown fox jumps over the lazy dog\r\n";
static uint32_t id;
/*******************************************************************************
 *******************************************************************************
						GLOBAL FUNCTION DEFINITIONS
*******************************************************************************/

static EVENT App_CaptureEvent();
/********************************************************************************
******************************************************************************/
/* interrupts are disabled at this point*/
void App_Init(void) {
	timer_drv_init();
	FSM_InitTable();

	id = UART_drv_instance_init(PORTNUM2PIN(PB, 16), PORTNUM2PIN(PB, 17));
	// initial state
	g_app_ctx.current_state = FSM_GetInitState();
}
static uint8_t offset = 0;
/* Función que se llama constantemente en un ciclo infinito */
void App_Run(void) {
	while (1) {
		timer_drv_update(); /* must be called every iteration */

		/**for (size_t i = 0; MSG[i] != '\0'; i++) {
			UART_polling_data_transmit(id, (uint8_t) MSG[i]);
		}
			**/
		if (UART_tstatus(id)) {
			uint8_t sent = UART_data_transmit(id, MSG + offset, 45 - offset);
			offset = (offset + sent) % 45; // resume from where we left off
		}

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
