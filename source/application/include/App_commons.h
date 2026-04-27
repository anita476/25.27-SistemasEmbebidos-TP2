#ifndef _APP_COMMONS_H_
#define _APP_COMMONS_H_
#include "fsm.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_ATTEMPTS 3
typedef struct {
	FSMState_t *current_state;
/**
* Add more as needed
*/

} AppContext_t;

extern AppContext_t g_app_ctx;


#endif /* _APP_COMMONS_H_ */