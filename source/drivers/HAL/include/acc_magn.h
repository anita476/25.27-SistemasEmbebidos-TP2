#ifndef _ACC_MAGN_H_
#define _ACC_MAGN_H_

#include <stdbool.h>
#include <stdint.h>

#define SENSOR_FREQUENCY_HZ 200						  // Sensor data rate in Hz
#define SENSOR_PERIOD_MS (2000 / SENSOR_FREQUENCY_HZ) // Sensor data rate in ms (hybrid mode)

typedef uint8_t byte_t;
typedef int16_t angle_t;

typedef struct {
	angle_t roll;
	angle_t pitch;
	// angle_t yaw;
} sensor_t;

/**
 * @brief Initializes accelerometer & magnetometer driver.
 * @note Needs interrupts enabled
 */
void acc_magn_drv_init(void);

/*
 * @brief Get accelerometer+magnetometer data
 * @returns A sensor type object containing roll, pitch and yaw angles.
 * @note Yaw was disabled, always reads 0!
 */
sensor_t *acc_magn_drv_get_angles(void);

#endif // _SENSOR_H_
