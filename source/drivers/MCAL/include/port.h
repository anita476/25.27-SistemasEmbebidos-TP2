#ifndef _PORT_H_
#define _PORT_H_
#include "../../../../SDK/startup/hardware.h"
#include <stdint.h>
#include "../../../../SDK/CMSIS/MK64F12.h"


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

#endif /*_PORT_H_*/