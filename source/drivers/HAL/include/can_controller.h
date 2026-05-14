#ifndef _CAN_CONTROLLER_H_
#define _CAN_CONTROLLER_H_
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize the can controller driver. Configures interruptiion pin and comm (spi0)
 * @note COMPLETELY BLOCKING AND NEEDS INTERRUPTS ENABLED
 */
bool can_controller_drv_init();

/**
 * @brief BLOCKING CAN TEST SENDER
 */
// bool can_send(const uint8_t *data, uint8_t len);

bool can_send(const uint8_t *data, uint8_t len, void (*on_done)(bool success));

void can_process(void);
#endif