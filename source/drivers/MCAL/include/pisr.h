/***************************************************************************/ /**
   @file     pisr.h
   @brief    Periodic Interrupt (PISR) driver
   @author   Nicolás Magliola
  ******************************************************************************/

#ifndef _PISR_H_
#define _PISR_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdbool.h>
#include <stdint.h>

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define PISR_FREQUENCY_HZ 1000U

#define PISR_CANT 8

#define TICK_MS 1

/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef void (*pisrCallbackPtr_t)(void);

/*******************************************************************************
 * VARIABLE PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/**
 * @brief Register PISR callback
 * @param fun PISR function to be call periodically
 * @param period PISR period in ticks
 * @return Registration succeed
 */
bool pisr_drv_register(pisrCallbackPtr_t fun, unsigned int period);

/*******************************************************************************
 ******************************************************************************/

#endif // _PISR_H_
