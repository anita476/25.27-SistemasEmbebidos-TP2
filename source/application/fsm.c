#include "include/fsm.h"
#include "include/App_commons.h"

/**
 * Using given fsm example
 */
FSMState_t *fsm(FSMState_t *p_state_table, EVENT curr_event) {
	FSMState_t *p_base = p_state_table; /* save base before walking */

	while (p_state_table->event != curr_event && p_state_table->event != TABLE_END) {
		++p_state_table;
	}
	//(*p_state_table->action_fun)();			   /* perform action routine */
	p_state_table->action_fun(); /* can update g_app_ctx.current_state if its a dynamic state */

	if (p_state_table->next_state != NULL) { // Obs! if its set by action, leave nxt state NULL in table!!!
		return p_state_table->next_state;
	}

	if (g_app_ctx.current_state != p_base) {
		return g_app_ctx.current_state;
	}

	return p_base; /* stay in current state, it will be the "modified one" by action */
}