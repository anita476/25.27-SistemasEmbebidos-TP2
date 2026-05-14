#ifndef _CAN_CONTROLLER_H
#define _CAN_CONTROLLER_H

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <stdbool.h>
#include <stdint.h>

/******************************************************************************
 * TYPES
 ******************************************************************************/

/**
 * @brief CAN frame container
 */
typedef struct {
	uint32_t id;
	uint8_t dlc;
	uint8_t data[8];
} CanFrame_t;
/**
 * @brief TX completion callback
 *
 * @param success true if frame transmitted successfully
 */
typedef void (*can_tx_cb_t)(bool success);

/******************************************************************************
 * PUBLIC API
 ******************************************************************************/

/**
 * @brief Initialize CAN controller driver
 *
 * Initializes:
 *  - SPI communication
 *  - MCP25625 registers
 *  - GPIO interrupt
 *
 * @return true on success
 */
bool can_controller_drv_init(void);

/**
 * @brief Main CAN driver processing function
 *
 * Must be called periodically from main loop.
 *
 * Handles:
 *  - completion of async CANINTF read
 *  - interrupt dispatching
 *  - TX completion
 *  - RX servicing
 *  - interrupt re-check
 */
void can_process(void);

/**
 * @brief Send CAN frame
 *
 * Non-blocking from application perspective.
 *
 * Returns false if:
 *  - TX buffer busy
 *  - interrupt servicing in progress
 *  - SPI unavailable
 *
 * @param data payload bytes
 * @param len payload length (0-8)
 * @param cb completion callback
 *
 * @return true if transmission started
 */
bool can_send(const uint8_t *data, uint8_t len, can_tx_cb_t cb);

/**
 * @brief Check whether RX frames are available
 *
 * @return true if at least one frame pending
 */
bool can_available(void);

/**
 * @brief Read next received CAN frame
 *
 * @param frame output frame
 *
 * @return true if frame returned
 */
bool can_read(CanFrame_t *frame);

/******************************************************************************
 * OPTIONAL STATUS HELPERS
 ******************************************************************************/

/**
 * @brief Returns true while TX pending
 */
bool can_tx_busy_status(void);

/**
 * @brief Returns true while interrupt servicing active
 */
bool can_irq_busy_status(void);

bool get_int_blocking(uint8_t *out);
#endif
