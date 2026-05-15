#include <math.h>
// #include "board.h"
// #include "debug.h"		// For measuring ISR time
#include "../MCAL/include/I2C.h"
// #include "macros.h"
#include "../MCAL/include/pisr.h"
// #include "timer.h"
#include "../HAL/include/FXOS.h"
#include <math.h>

// #define DEVELOPMENT_MODE			1
#define CONFIG_FREQUENCY_HZ 1000U

// typedef enum { OFF, IDLE, READING } state_t;
// typedef enum { BLOCKING, NON_BLOCKING } mode_t;
// typedef enum { BUSY, DONE, ERROR } bus_status_t;

// static const bus_status_t bus_status[] = { BUSY, DONE, ERROR };

static raw_data_t accel_data, magn_data;
static sensor_t data;
// static bool status_bus, status_config;
uint8_t databuffer[FXOS8700CQ_READ_LEN] = {0};

sensor_t *FXOSgetAngles(void) {
	return &data;
}

void FXOS_Init(void) {
	static uint8_t ID;
	uint8_t databyte;

	I2C_Init();

	if (I2C_GetStatus() == Done) {
		I2C_StartComm(&ID, 1, FXOS8700CQ_ADDR, FXOS8700CQ_WHOAMI, Read); // Read and check the ID
		waitforI2C();
		if (ID == FXOS8700CQ_WHOAMI_VAL) {
			databyte = 0x00; // Place into standby
			I2C_StartComm(&databyte, 1, FXOS8700CQ_ADDR, FXOS8700CQ_CTRL_REG1, Write);
			waitforI2C();
			databyte = 0x1F; // No auto calibration, one-shot magn reset or measurement, 8x oversampling and hybrid mode
			I2C_StartComm(&databyte, 1, FXOS8700CQ_ADDR, FXOS8700CQ_M_CTRL_REG1, Write);
			waitforI2C();
			databyte =
				0x20; // Map magn registers to follow accel, retain min/max latching and enable magn reset each cycle
			I2C_StartComm(&databyte, 1, FXOS8700CQ_ADDR, FXOS8700CQ_M_CTRL_REG2, Write);
			waitforI2C();
			databyte = 0x01; // No filter and accel range of +/-4g range with 0.488mg/LSB
			I2C_StartComm(&databyte, 1, FXOS8700CQ_ADDR, FXOS8700CQ_XYZ_DATA_CFG, Write);
			waitforI2C();
			// databyte = 0x00; // Disable FIFO
			// I2C_StartComm(&databyte, 1, FXOS8700CQ_ADDR, FXOS8700CQ_F_SETUP, Write);
			// waitforI2C();
			databyte = 0x0D; // 200Hz data rate, low noise, 16 bit reads, out of standby and enable sampling
			I2C_StartComm(&databyte, 1, FXOS8700CQ_ADDR, FXOS8700CQ_CTRL_REG1, Write);
			waitforI2C();

			pisr_drv_register(readFXOSdata, 25); // Configure PISR for the FXOS
		}
	}
}

void readFXOSdata(void) {
	// Process the data from the last read cycle
	// accel_data.x = (int16_t)((databuffer[1] << 8) | databuffer[2]) >> 2;
	// accel_data.y = (int16_t)((databuffer[3] << 8) | databuffer[4]) >> 2;
	// accel_data.z = (int16_t)((databuffer[5] << 8) | databuffer[6]) >> 2;

	// magn_data.x = (int16_t)((databuffer[7]  << 8) | databuffer[8]);
	// magn_data.y = (int16_t)((databuffer[9]  << 8) | databuffer[10]);
	// magn_data.z = (int16_t)((databuffer[11] << 8) | databuffer[12]);
	accel_data.x = (int16_t) (((databuffer[1] << 8) | databuffer[2])) >> 2;
	accel_data.y = (int16_t) (((databuffer[3] << 8) | databuffer[4])) >> 2;
	accel_data.z = (int16_t) (((databuffer[5] << 8) | databuffer[6])) >> 2;

	magn_data.x = (databuffer[7] << 8) | databuffer[8];
	magn_data.y = (databuffer[9] << 8) | databuffer[10];
	magn_data.z = (databuffer[11] << 8) | databuffer[12];

	// Calculate Y,R,P angles from axis data
	float pitch_rad = atan2f(accel_data.y, accel_data.z);
	float roll_rad = atan2f(accel_data.x, accel_data.z);

	float mag_x_comp = magn_data.x * cosf(pitch_rad) + magn_data.z * sinf(pitch_rad);
	float mag_y_comp = magn_data.x * sinf(roll_rad) * sinf(pitch_rad) + magn_data.y * cosf(roll_rad) -
					   magn_data.z * sinf(roll_rad) * cosf(pitch_rad);

	data.yaw = (atan2f(mag_y_comp, mag_x_comp) * 180.0f / (float) M_PI);
	data.pitch = pitch_rad * 180.0f / (float) M_PI;
	data.roll = roll_rad * 180.0f / (float) M_PI;
	// data.roll = atan2f(accel_data.y, accel_data.z) * 180/M_PI;
	// data.pitch = atan2f((-1)* accel_data.x, (sqrt(accel_data.y * accel_data.y + accel_data.z * accel_data.z))) *
	// 180/M_PI;

	// Start next read cycle
	I2C_StartComm(databuffer, FXOS8700CQ_READ_LEN, FXOS8700CQ_ADDR, FXOS8700CQ_STATUS, Read);
}

void waitforI2C() {
	I2C_Status_t status;
	while ((status = I2C_GetStatus()) == Busy) {
	}
}
