#include "include/can_controller.h"
#include "../MCAL/include/gpio.h"
#include "../MCAL/include/spi.h"
#include "include/board.h"

bool can_controller_drv_init() {
	// initialize spi
	if (!spi_drv_init(0, SPI_BAUDRATE)) {
		return false;
	}

	// set up gpio !
	/**
	 * according to MCP25625:
	 * When an interrupt occurs,
	 * the INT pin is driven low by the MCP25625
	 * and will remain low until the interrupt is cleared by the MCU.
	 * An interrupt can not be cleared if the respective condition still prevails.
	 */
	gpio_drv_mode(PIN_CAN_INT, INPUT_PULLUP); //
	if (!gpio_drv_IRQ(PIN_CAN_INT, GPIO_IRQ_MODE_FALLING_EDGE, can_gpio_irq)) {
		return false;
	}
	// disable while setting up CAN
	NVIC_DisableIRQ(port_irqs[PIN2PORT(PIN_CAN_INT)]);

	// set up can registers !

	NVIC_EnableIRQ(port_irqs[PIN2PORT(PIN_CAN_INT)]);
	return true;
}

/*****************************gpio interrupt*************************************/
void can_gpio_irq(void) {
	;
}
/**************************************HELPERS***********************************/
