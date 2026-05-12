#ifndef _CAN_CONTROLLER_H_
#define _CAN_CONTROLLER_H_
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize the can controller driver. Configures interruptiion pin and comm (spi0)
 * @note COMPLETELY BLOCKING AND NEEDS INTERRUPTS ENABLED
 */
bool can_controller_drv_init();

#endif