#ifndef _BOARD_LED_H_
#define _BOARD_LED_H_
#include <stdbool.h>

#define COLOR_NUM 3
typedef enum { RED, GREEN, BLUE } Color;

/**
 * @brief initializes the board led driver
 */
void board_led_drv_init();

/**
 * @brief Manages on board led state for a particular color
 * @param color Led color
 * @param state True for on, false for off
 */
void board_led_drv_state(Color color, bool state);

#endif