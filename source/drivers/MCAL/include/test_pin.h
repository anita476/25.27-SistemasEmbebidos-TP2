#ifndef _TEST_PIN_H_
#define _TEST_PIN_H_
#include "gpio.h"

#define TP PORTNUM2PIN(PB, 2) /* test pin */

/**** TECHNICALLY isnt mcal, but bc its only associated with gpio and pisr @todo maybe change?  */
void test_pin_init(void);

#endif