/***************************************************************************/ /**
   @file     timer.h
   @brief    Timer driver. Advance implementation, Non-Blocking services
   @author   Nicolás Magliola
  ******************************************************************************/

#ifndef _TIMER_H_
#define _TIMER_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdbool.h>
#include <stdint.h>

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/
// How many pisr ints should pass untill calling timer
#define TIMER_PERIOD 1

#define TIMER_TICK_MS (TICK_MS * TIMER_PERIOD)
#define TIMER_MS2TICKS(ms) ((ms) / TIMER_TICK_MS)

#define TIMERS_MAX_CANT 16
#define TIMER_INVALID_ID 255

/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

// Timer Modes
enum { TIM_MODE_SINGLESHOT, TIM_MODE_PERIODIC, CANT_TIM_MODES };

// Timer alias
typedef uint32_t timTick_t;
typedef uint8_t timId_t;
typedef void (*timCallback_t)(void);

/*******************************************************************************
 * VARIABLE PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/**
 * @brief Initialice timer and corresponding peripheral
 */
void timer_drv_init(void);

/**
 * @brief Request an timer
 * @return ID of the timer to use
 */
timId_t timer_drv_get_id(void);

/**
 * @brief Begin to run a new timer
 * @param id ID of the timer to start
 * @param ticks time until timer expires, in ticks
 * @param mode SINGLESHOT or PERIODIC
 * @param callback Function to be call when timer expires (NULL if no necessary)
 * @return true = timer start succeed
 */
bool timer_drv_start(timId_t id, timTick_t ticks, uint8_t mode, timCallback_t callback);

/**
 * @brief Finish to run a timer
 * @param id ID of the timer to stop
 */
void timer_drv_stop(timId_t id);

/**
 * @brief Verify if a timer has run timeout
 * @param id ID of the timer to check for expiration
 * @return true = timer expired
 */
bool timer_drv_expired(timId_t id);

/**
 * @brief Call respective callbacks if timeout ocurrs. Must be call from main loop.
 */
void timer_drv_update(void);

/**
 * @brief Delete a timer (frees its space). Especially for period interrupts. To use another timer must request a new
 * one.
 * @param id Id of the timer to delete
 */
void timer_drv_delete(timId_t id);

/*******************************************************************************
 ******************************************************************************/

#endif // _TIMER_H_
