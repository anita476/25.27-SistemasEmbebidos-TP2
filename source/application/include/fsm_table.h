/*
 * fsmtable.h
 *
 *  Created on: 28/07/2014
 *      Author: Daniel Jacoby
 *
 * (Modified)
 */

#ifndef _FSMTABLE_H_
#define _FSMTABLE_H_
#include "fsm.h"
#include <stddef.h>
void FSM_InitTable(void);
FSMState_t *FSM_GetInitState(void);

#endif /* _FSMTABLE_H_ */