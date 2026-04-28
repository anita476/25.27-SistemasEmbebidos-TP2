#ifndef _UART_H_
#define _UART_H_
#include "gpio.h"
#include <stdbool.h>
#include <stdint.h>

#define UART_COUNT 5
#define INVALID_UART 255
/******************** NON BLOCKING UART DRV IMPLEMENTATION ******************************/
/**
 * @brief Initialize uart instance
 * @returns A number representing uart id, or INVALID_UART if unsuccessful
 * @param RX_pin Receiver pin
 * @param TX_pin Transciever pin
 **/
uint32_t UART_drv_instance_init(pin_t RX_pin, pin_t TX_pin);

/**
 * @brief Query the read status of a uart
 * @param uart_id The uart to query
 * @returns True if theres pending data to be read, false if not
 **/
bool UART_rstatus(uint8_t uart_id);

/**
 *
 * @brief Query the transmit status of a uart
 * @param uart_id The uart id
 * @returns True if data can be transmitted, false if not.
 */
bool UART_tstatus(uint8_t uart_id);

/**
 * @brief Set baudrate for a uart number (uarts go form 0 to 4)
 **/
void UART_set_baudrate(uint8_t uart_id, uint32_t baudrate);

/**
 * @brief Queue data to send. (Non blocking)
 * @param txdata Data array to send
 * @param length Its length
 * @returns The number of bytes that were sent
 **/
uint8_t UART_data_transmit(uint8_t uart_id, unsigned char *txdata, uint8_t length);
/**
 * @brief Receive data if available. This clears the read status
 * @param out The out buffer to which the data is copied
 * @returns The number of bytes read into the buffer. 0 if no data was available.
 **/
uint8_t UART_data_receive(uint8_t uart_id, uint8_t *out, uint8_t length);
#endif /*_UART_NONBLOCKING_FIFO_H_*/