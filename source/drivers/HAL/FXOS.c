#include "../HAL/include/FXOS.h"
#include "../MCAL/include/I2C.h"

// FXOS I2C address
#define FXOS_ADDR 0x1E // with pins SA0=0, SA1=0

// FXOS internal register addresses
#define FXOS_STATUS 0x00
#define FXOS_WHOAMI 0x0D
#define FXOS_XYZ_DATA_CFG 0x0E
#define FXOS_CTRL_REG1 0x2A
#define FXOS_M_CTRL_REG1 0x5B
#define FXOS_M_CTRL_REG2 0x5C
#define FXOS_WHOAMI_VAL 0xC7

// number of bytes to be read from the FXOS
#define FXOS_READ_LEN 13 // status plus 6 channels =13 bytes

// function configures FXOS combination accelerometer and magnetometer sensor
int FXOS_Init(void) {
	I2C_Init();
	uint8_t data[FXOS_READ_LEN];

	// read and check the FXOS WHOAMI register
	if (I2C_StartComm(data, 1, FXOS_ADDR, FXOS_WHOAMI, Read) != 1) {
		return (FXOS_ERROR);
	}
	if (data[0] != FXOS_WHOAMI_VAL) {
		return (FXOS_ERROR);
	}

	// write 0000 0000 = 0x00 to accelerometer control register 1 to place FXOS into standby
	data[0] = 0x00;
	if (I2C_StartComm(data, 1, FXOS_ADDR, FXOS_CTRL_REG1, Write) != 1) {
		return (FXOS_ERROR);
	}

	// write 0001 1111 = 0x1F to magnetometer control register 1
	data[0] = 0x1F;
	if (I2C_StartComm(data, 1, FXOS_ADDR, FXOS_M_CTRL_REG1, Write) != 1) {
		return (FXOS_ERROR);
	}

	// write 0010 0000 = 0x20 to magnetometer control register 2
	data[0] = 0x20;
	if (I2C_StartComm(data, 1, FXOS_ADDR, FXOS_M_CTRL_REG2, Write) != 1) {
		return (FXOS_ERROR);
	}

	// write 0000 0001= 0x01 to XYZ_DATA_CFG register
	data[0] = 0x01;
	if (I2C_StartComm(data, 1, FXOS_ADDR, FXOS_XYZ_DATA_CFG, Write) != 1) {
		return (FXOS_ERROR);
	}

	// write 0000 1101 = 0x0D to accelerometer control register 1
	data[0] = 0x0D;
	if (I2C_StartComm(data, 1, FXOS_ADDR, FXOS_CTRL_REG1, Write) != 1) {
		return (FXOS_ERROR);
	}
	// normal return
	return (FXOS_OK);
}

// read status and the three channels of accelerometer andmagnetometer data from FXOS (13 bytes)
FXOS_Status_t ReadAccelMagnData(SRAWDATA *pAccelData, SRAWDATA *pMagnData) {
	uint8_t Buffer[FXOS_READ_LEN]; // read buffer
	// read FXOS_READ_LEN=13 bytes (status byte and the sixchannels of data)
	if (I2C_StartComm(Buffer, FXOS_READ_LEN, FXOS_ADDR, FXOS_STATUS, Read) == FXOS_READ_LEN) {
		// copy the 14 bit accelerometer byte data into 16 bit words
		pAccelData->x = (int16_t) (((Buffer[1] << 8) | Buffer[2])) >> 2;
		pAccelData->y = (int16_t) (((Buffer[3] << 8) | Buffer[4])) >> 2;
		pAccelData->z = (int16_t) (((Buffer[5] << 8) | Buffer[6])) >> 2;
		// copy the magnetometer byte data into 16 bit words
		pMagnData->x = (Buffer[7] << 8) | Buffer[8];
		pMagnData->y = (Buffer[9] << 8) | Buffer[10];
		pMagnData->z = (Buffer[11] << 8) | Buffer[12];
	} else {
		// return with error
		return (FXOS_ERROR);
	}
	// normal return
	return (FXOS_OK);
}