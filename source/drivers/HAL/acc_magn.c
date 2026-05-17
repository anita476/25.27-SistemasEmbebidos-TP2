#include "../HAL/include/acc_magn.h"
#include "../../../SDK/CMSIS/cmsis_gcc.h"
#include "../MCAL/include/i2c.h"
#include "../MCAL/include/pisr.h"
#include <math.h>

#define CONFIG_FREQUENCY_HZ 1000U

typedef enum {
	FXOS_RUNNING,
	acc_magn_drv_init_START,
	acc_magn_drv_init_WHOAMI,
	acc_magn_drv_init_STANDBY,
	acc_magn_drv_init_M_CTRL1,
	acc_magn_drv_init_M_CTRL2,
	acc_magn_drv_init_XYZ_CFG,
	acc_magn_drv_init_ACTIVE,
} AccState_t;

typedef struct {
	int16_t x;
	int16_t y;
	int16_t z;
} RawData_t;

static RawData_t accel_data, magn_data;
static sensor_t data;
uint8_t databuffer[FXOS8700CQ_READ_LEN] = {0};

static volatile AccState_t fxos_state = acc_magn_drv_init_START;
static uint8_t databyte;
static uint8_t ID;

void _acc_magn_task(void);
void _acc_magn_read_data(void);

void acc_magn_drv_init(void) {
	i2c_drv_init();
	fxos_state = acc_magn_drv_init_START;
	pisr_drv_register(_acc_magn_task, 25);
}

sensor_t *acc_magn_drv_get_angles(void) {
	return &data;
}

void _acc_magn_task(void) {
	if (i2c_get_status() == Busy)
		return; // wait for previous transaction

	switch (fxos_state) {
		case FXOS_RUNNING:
			_acc_magn_read_data();
			break;
		case acc_magn_drv_init_START:
			i2c_drv_start_comm(&ID, 1, FXOS8700CQ_ADDR, FXOS8700CQ_WHOAMI, Read);
			fxos_state = acc_magn_drv_init_WHOAMI;
			break;

		case acc_magn_drv_init_WHOAMI:
			if (ID != FXOS8700CQ_WHOAMI_VAL) { /* handle error */
				break;
			}
			databyte = 0x00;
			i2c_drv_start_comm(&databyte, 1, FXOS8700CQ_ADDR, FXOS8700CQ_CTRL_REG1, Write);
			fxos_state = acc_magn_drv_init_STANDBY;
			break;

		case acc_magn_drv_init_STANDBY:
			databyte = 0x9F; // Habilitar autocalibracion para que funcione bien el magnetometro
			i2c_drv_start_comm(&databyte, 1, FXOS8700CQ_ADDR, FXOS8700CQ_M_CTRL_REG1, Write);
			fxos_state = acc_magn_drv_init_M_CTRL1;
			break;

		case acc_magn_drv_init_M_CTRL1:
			databyte = 0x20;
			i2c_drv_start_comm(&databyte, 1, FXOS8700CQ_ADDR, FXOS8700CQ_M_CTRL_REG2, Write);
			fxos_state = acc_magn_drv_init_M_CTRL2;
			break;

		case acc_magn_drv_init_M_CTRL2:
			databyte = 0x01;
			i2c_drv_start_comm(&databyte, 1, FXOS8700CQ_ADDR, FXOS8700CQ_XYZ_DATA_CFG, Write);
			fxos_state = acc_magn_drv_init_XYZ_CFG;
			break;

		case acc_magn_drv_init_XYZ_CFG:
			databyte = 0x0D;
			i2c_drv_start_comm(&databyte, 1, FXOS8700CQ_ADDR, FXOS8700CQ_CTRL_REG1, Write);
			fxos_state = acc_magn_drv_init_ACTIVE;
			break;

		case acc_magn_drv_init_ACTIVE:
			// Start the first I2C read cycle before moving to RUNNING state
			i2c_drv_start_comm(databuffer, FXOS8700CQ_READ_LEN, FXOS8700CQ_ADDR, FXOS8700CQ_STATUS, Read);
			fxos_state = FXOS_RUNNING;
			break;
	}
}

void _acc_magn_read_data(void) {
	// Process the data from the last read cycle
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

	// data.yaw = (atan2f(mag_y_comp, mag_x_comp) * 180.0f / (float) M_PI);
	// leave yaw incomplete for now ...
	data.yaw = 0;
	data.pitch = ((-1) * pitch_rad * 180.0f / (float) M_PI);
	data.roll = roll_rad * 180.0f / (float) M_PI;

	// Start next read cycle
	i2c_drv_start_comm(databuffer, FXOS8700CQ_READ_LEN, FXOS8700CQ_ADDR, FXOS8700CQ_STATUS, Read);
}