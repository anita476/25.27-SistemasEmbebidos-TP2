#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// #define BAUDRATE 115200
#define BAUDRATE 9600
#define BUFFER_SIZE 64
#define CTRL_A 0x01

/*
 * @brief Test a certain uart port. Assumes the uart is already initialized
 *
 */
bool uart_test(uint8_t uart_id);
