#ifndef _TEST_PIN_H_
#define _TEST_PIN_H_
#include "gpio.h"

#define TP PORTNUM2PIN(PB, 2)	   /* test pin */
#define UART_TP PORTNUM2PIN(PB, 3) /* for uart interrupt*/
#define SPI_TP PORTNUM2PIN(PB, 10)
#define GPIO_TP PORTNUM2PIN(PB, 11) /* for port interrupts in gpio*/

/**** TECHNICALLY isnt mcal, but bc its only associated with gpio and pisr @todo maybe change?  */
void tp_generic_init(void);

void tp_gpio_init(void);

void tp_uart_init(void);

void tp_spi_init(void); /* for spi specific interrupt*/

#endif