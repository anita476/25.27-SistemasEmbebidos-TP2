/***************************************************************************/ /**
   @file     board.h
   @brief    Board management
   @author   Nicolás Magliola
  ******************************************************************************/

#ifndef _BOARD_H_
#define _BOARD_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "../../MCAL/include/gpio.h"

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

/***** BOARD defines **********************************************************/

// On Board User LEDs
#define PIN_LED_RED PORTNUM2PIN(PB, 22)	  // PTB22
#define PIN_LED_GREEN PORTNUM2PIN(PE, 26) // PTE26
#define PIN_LED_BLUE PORTNUM2PIN(PB, 21)  // PTB21
#define LED_ACTIVE LOW

// On Board User Switches
#define PIN_SW2 PORTNUM2PIN(PC, 6) // PTC6
#define PIN_SW3 PORTNUM2PIN(PA, 4) // PTA4

// Channel & switch pins for encoder
#define PIN_ENC_CHNA PORTNUM2PIN(PC, 3)
#define PIN_ENC_CHNB PORTNUM2PIN(PC, 2)
#define PIN_SW_ENC PORTNUM2PIN(PA, 2)

// DEnc board status leds selection pins
#define PIN_U1B_STATUS0 PORTNUM2PIN(PC, 7)
#define PIN_U1B_STATUS1 PORTNUM2PIN(PC, 0)

// DEnc board digit display selection pins
#define PIN_U1A_SEL0 PORTNUM2PIN(PC, 9)
#define PIN_U1A_SEL1 PORTNUM2PIN(PC, 8)

// Magnetic Card Reader pins
#define PIN_CARD_DATA PORTNUM2PIN(PB, 23)
#define PIN_CARD_CLOCK PORTNUM2PIN(PA, 1)
#define PIN_CARD_ENABLE PORTNUM2PIN(PB, 9)

#define DEC_ACTIVE LOW

#define SW_ACTIVE LOW
#define SW_INPUT_TYPE INPUT_PULLUP // en realidad para sw3 no hace falta, para sw2 SI

#define DISPLAY_PINS 8
#define DISPLAY_SEG_ACTIVE HIGH

#define SR_DATA PORTNUM2PIN(PD, 1) // serial data in
#define SR_SCLK PORTNUM2PIN(PD, 0) // shift clock
#define SR_LATCH PORTNUM2PIN(PD, 2)
#define SR_DEC_OE PORTNUM2PIN(PD, 3) // output enable for selectors! (probs unused ..)
#define SR_SEG_OE PORTNUM2PIN(PC, 4) // output enable for SEGMENTS

#endif /* _BOARD_H_ */
