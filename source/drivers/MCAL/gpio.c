#include "include/gpio.h"
#include "../../../SDK/CMSIS/MK64F12.h"
#include "../../../SDK/startup/hardware.h"
#include "include/test_pin.h"
#include <stdint.h>

/*
 * @file gpio.c
 * @brief Driver implementation for gpio access
 *
 * */

#define PULLDOWN 0
#define PULLUP 1

// map user irq modes to actual bits needed in pcr irqc
#define IRQ_MODE_TO_IRQC(mode) ((mode) == GPIO_IRQ_MODE_DISABLE ? PORT_eDisabled : (0x08 | (mode)))

// Lookup for port masks
static const uint32_t port_clock_masks[] = {
	SIM_SCGC5_PORTA_MASK, SIM_SCGC5_PORTB_MASK, SIM_SCGC5_PORTC_MASK, SIM_SCGC5_PORTD_MASK, SIM_SCGC5_PORTE_MASK,
};
// lookup for port access
static PORT_Type *const port_ptrs[] = PORT_BASE_PTRS;
static GPIO_Type *const gpio_ptrs[] = GPIO_BASE_PTRS;

// lookup for port irq access
static const IRQn_Type port_irqs[] = {PORTA_IRQn, PORTB_IRQn, PORTC_IRQn, PORTD_IRQn, PORTE_IRQn};

typedef enum {
	PORT_mAnalog,
	PORT_mGPIO,
	PORT_mAlt2,
	PORT_mAlt3,
	PORT_mAlt4,
	PORT_mAlt5,
	PORT_mAlt6,
	PORT_mAlt7,

} PORTMux_t; // alternatives for PORT peripherals

typedef enum {
	PORT_eDisabled = 0x00,
	PORT_eDMARising = 0x01,
	PORT_eDMAFalling = 0x02,
	PORT_eDMAEither = 0x03,
	PORT_eInterruptDisasserted = 0x08,
	PORT_eInterruptRising = 0x09,
	PORT_eInterruptFalling = 0x0A,
	PORT_eInterruptEither = 0x0B,
	PORT_eInterruptAsserted = 0x0C,
} PORTEvent_t;

#define MAX_CBCKS_PORT 5
#define PORT_NUM 5
// prettify
typedef struct {
	pin_t pin;
	pinIrqFun_t callback_fn;
} gpioPinCallback_t;

typedef struct {
	uint32_t used; // counter to track # of used callbacks
	gpioPinCallback_t callbacks[MAX_CBCKS_PORT];
} gpioPortCallbacks_t;

static gpioPortCallbacks_t irqCallbacks[PORT_NUM]; // here we will store the callback

static void _execute_callbacks(int port);

void gpio_drv_mode(pin_t pin, uint8_t mode) {
	int port_num = PIN2PORT(pin);
	int port_pin_num = PIN2NUM(pin);
	/*
	 *
	 * To configure the pin as gpio first i must enable clocking for the port
	 *
	 * */
	SIM->SCGC5 |= port_clock_masks[port_num];

	/* Building pcr
	 */
	uint32_t pcr = PORT_PCR_MUX(PORT_mGPIO) | PORT_PCR_DSE(1) | PORT_PCR_SRE(1) |
				   PORT_PCR_IRQC(PORT_eDisabled); // disable interrupts

	if (mode == INPUT_PULLDOWN) {
		pcr |= PORT_PCR_PE(1);		  // pull enable
		pcr |= PORT_PCR_PS(PULLDOWN); // pull select
	} else if (mode == INPUT_PULLUP) {
		pcr |= PORT_PCR_PE(1);
		pcr |= PORT_PCR_PS(PULLUP);
	}

	/* assign pcr*/
	port_ptrs[port_num]->PCR[port_pin_num] = pcr;

	/* now we can configure it as output, input */
	if (mode == OUTPUT) {
		gpio_ptrs[port_num]->PDDR |= (1u << port_pin_num);
	} else {
		gpio_ptrs[port_num]->PDDR &= ~(1u << port_pin_num);
	}
}

/* Obs! Remember that PSOR only SETS bits. To write a 0 we need to use PCOR
 *  PSOR/PCOR/PTOR are write only , so the |= is not needed, only = is fine
 */
void gpio_drv_write(pin_t pin, bool value) {
	int port_num = PIN2PORT(pin);
	int port_pin_num = PIN2NUM(pin);

	/* check if dd is output  first*/
	if (gpio_ptrs[port_num]->PDDR & (1 << port_pin_num)) {
		if (value) {
			gpio_ptrs[port_num]->PSOR = (1 << port_pin_num);
		} else {
			gpio_ptrs[port_num]->PCOR = (1 << port_pin_num);
		}
	}
}

void gpio_drv_toggle(pin_t pin) {
	gpio_ptrs[PIN2PORT(pin)]->PTOR = (1 << PIN2NUM(pin));
}

bool gpio_drv_read(pin_t pin) {
	return (gpio_ptrs[PIN2PORT(pin)]->PDIR >> PIN2NUM(pin)) & 1u;
}

/**
Obs! We need to store the callbacks for each pin
2 approaches -> either store a full matrix with access via index por each PORT-PIN
						-> or store a maximum array of structures that contain each PIN+CALLBACK
 */
bool gpio_drv_IRQ(pin_t pin, uint8_t irqMode, pinIrqFun_t irqFun) {
	int port = PIN2PORT(pin);
	int portPin = PIN2NUM(pin);
	if (irqCallbacks[port].used >= MAX_CBCKS_PORT) {
		return false;
	}
	// enable nvic interrupts for that port
	NVIC_EnableIRQ(port_irqs[port]);
	// enable interrupts for that pins pcr
	// Obs!
	port_ptrs[port]->PCR[portPin] =
		(port_ptrs[port]->PCR[portPin] & ~PORT_PCR_IRQC_MASK) | PORT_PCR_IRQC(IRQ_MODE_TO_IRQC(irqMode));

	irqCallbacks[port].callbacks[irqCallbacks[port].used].callback_fn = irqFun;
	irqCallbacks[port].callbacks[irqCallbacks[port].used].pin = PIN2NUM(pin); // Obs! port pin is stored
	irqCallbacks[port].used++; // if all portX irqs have the same prio it should be fine -> no concurrency

	return true;
}

void PORTA_IRQHandler(void) {
	_execute_callbacks(PA);
}
void PORTB_IRQHandler(void) {
	_execute_callbacks(PB);
}
void PORTC_IRQHandler(void) {
	_execute_callbacks(PC);
}
void PORTD_IRQHandler(void) {
	_execute_callbacks(PD);
}
void PORTE_IRQHandler(void) {
	_execute_callbacks(PE);
}

static void _execute_callbacks(int port) {
	/* TEST PIN */
	gpio_drv_write(TP, HIGH);
	/*********** */
	// Obs! In this case i dont need to cycle through the whole PORTX_ISFR, just checking the actual interrupts
	// configured and checking isr is enough
	int i = irqCallbacks[port].used;
	while (i) {
		int pin = irqCallbacks[port].callbacks[i - 1].pin;
		// check the pins isr, clear it, and execute callback
		if (port_ptrs[port]->PCR[pin] & PORT_PCR_ISF_MASK) {
			port_ptrs[port]->PCR[pin] |= PORT_PCR_ISF_MASK; // w1c!!
			irqCallbacks[port].callbacks[i - 1].callback_fn();
		}
		i--;
	}
	gpio_drv_write(TP, LOW);
}
