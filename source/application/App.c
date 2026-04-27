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
#include "include/App_commons.h"
#include "include/fsm_table.h"
/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

/**
 * global variable, will be used by fsm
 * since we are working sequentially and interrupts dont access it or use it, it should be safe
 * */
AppContext_t g_app_ctx = {.current_state = NULL, // set after initing tabl
};
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

	// initial state
	g_app_ctx.current_state = FSM_GetInitState();
}

/* Función que se llama constantemente en un ciclo infinito */
void App_Run(void) {
	while (1) {
		timer_drv_update(); /* must be called every iteration */

		// Capture ONE event from all input sources
		EVENT curr_event = App_CaptureEvent();

		// Feed event to FSM if theres something
		if (curr_event != EV_NONE) {
			g_app_ctx.current_state = fsm(g_app_ctx.current_state, curr_event);
		}
	}
}

static EVENT App_CaptureEvent() {
	/**
	* Capture events as they appear
	**/
	return EV_NONE;
}
