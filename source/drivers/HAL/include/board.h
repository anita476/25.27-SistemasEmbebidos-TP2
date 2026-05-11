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

#define PIN_UART0_RX PORTNUM2PIN(PB, 16) // PTB16
#define PIN_UART0_TX PORTNUM2PIN(PB, 17) // PTB17
#define UART0_BAUDRATE 115200

// On Board User Switches
#define PIN_SW2 PORTNUM2PIN(PC, 6) // PTC6
#define PIN_SW3 PORTNUM2PIN(PA, 4) // PTA4
#define SW_ACTIVE LOW
#define SW_INPUT_TYPE INPUT_PULLUP // en realidad para sw3 no hace falta, para sw2 SI

#endif /* _BOARD_H_ */
