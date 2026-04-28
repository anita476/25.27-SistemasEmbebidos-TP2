#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// #define BAUDRATE 115200
#define BAUDRATE 9600
#define BUFFER_SIZE 64
#define CTRL_A 0x01

bool uart_test(void);
