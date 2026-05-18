#include "include/test_pin.h"
void tp_generic_init(void) { /* for systick + i2c port driven interrupt!*/
	gpio_drv_mode(TP, OUTPUT);
}
void tp_can_init(void) {
	gpio_drv_mode(CAN_TP, OUTPUT);
}

void tp_uart_init(void) { /* for uart specific tx_rx interrupt*/
	gpio_drv_mode(UART_TP, OUTPUT);
}

void tp_spi_init(void) { /* for spi specific interrupt*/
	gpio_drv_mode(SPI_TP, OUTPUT);
}

void tp_i2c_init(void) {
	gpio_drv_mode(I2C_TP, OUTPUT);
}
/* can can be tested with the gpio one! */