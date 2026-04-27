#ifndef _FSM_H_
#define _FSM_H_

#define TABLE_END 0xFF

// Forward declaration for self-referencing struct
typedef struct FSMState_t FSMState_t;

// has to be a user or pisr
typedef enum {
	EV_CLICK,
	EV_DOUBLE_CLICK,
	EV_LONG_CLICK,
	EV_TIMEOUT,
	EV_NONE
} EVENT;

struct FSMState_t {
	EVENT event;
	FSMState_t *next_state;
	void (*action_fun)(void); /* callback action */
};

/** fsm entrypoint */
FSMState_t *fsm(FSMState_t *p_state_table, EVENT curr_event);

#endif /* _FSM_H_ */