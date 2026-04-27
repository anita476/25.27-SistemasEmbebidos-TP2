#ifndef _SWITCH_H_
#define _SWITCH_H_

#include "../../MCAL/include/gpio.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SW_PISR_PERIOD 5

#define SW_MAX_PENDING_EVENTS 32
#define SW_MAX_SWS 8		  // max switches that can be registered
#define INVALID_SW_HANDLE 255 // idem timer....

#define SW_DBC_MS
typedef uint8_t sw_handle_t;
typedef enum : uint8_t { SW_EVENT_NONE = 0, SW_EVENT_CLICK, SW_EVENT_DOUBLE_CLICK, SW_EVENT_LONG_CLICK } swEventType;

typedef enum { ACTIVE_ON_LOW = LOW, ACTIVE_ON_HIGH = HIGH } ACTIVE_ON;
typedef enum { PULL_NONE = 0, PULL_DOWN, PULL_UP } PULL;

typedef struct {
	uint8_t swPin;
	swEventType event_type;
} swEvent;

/**
 * @brief Initialize the switch driver
 */
void switch_drv_init();
/**
 * @brief Register a switch with the corresponding pin,
 * @return True if successful, false if not
 * @param pin Absolute mcu pin for switch
 * @param active_i¿on Active on low or high
 * @param pullconfig Pull configuration
 **/
sw_handle_t switch_drv_register(uint8_t pin, ACTIVE_ON active_level, PULL pullconfig);
/**
 * @brief Unregister a switch
 * @param handle Switch handle, delivered at registration
 */
void switch_drv_unregister(sw_handle_t handle);

/**
 * @brief Pop an event from the switch event queue. Queue is circular with max SW_MAX_PENDING_EVENTS, otherwise
 * overwrites
 * @returns A switch event, containing the event type and the sw pin that triggered it
 */
swEvent switch_drv_pop_event();

#endif // _SWITCH_H_