#include "include/board_led.h"
#include "../MCAL/include/gpio.h"
#include "include/board.h"

static const int leds[COLOR_NUM] = {PIN_LED_RED, PIN_LED_GREEN, PIN_LED_BLUE};

/**
 * @brief initializes the board led driver
 */
void board_led_drv_init() {
	int i = 0;
	while (i != COLOR_NUM) {
		gpio_drv_mode(leds[i], OUTPUT);
		gpio_drv_write(leds[i], !LED_ACTIVE);
		i++;
	}
}

/**
 * @brief Manages on board led state for a particular color
 * @param color Led color
 * @param state True for on, false for off
 */
void board_led_drv_state(Color color, bool state) {
	gpio_drv_write(leds[color], (state ? LED_ACTIVE : (!LED_ACTIVE)));
}