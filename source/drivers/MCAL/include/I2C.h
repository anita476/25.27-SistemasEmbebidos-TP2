#ifndef _I2C_H_
#define _I2C_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	Read,
	Write,
} I2CAction_t;

typedef enum {
	Busy, // Currently transmitting or receiving
	Done, // Already finished communication
	Error // Some error occurred
} I2CStatus_t;

typedef uint8_t I2CAddress_t;

typedef struct {
	volatile I2CStatus_t status;
	I2CAction_t RW;
	uint8_t *data_arr;
	uint8_t size;
	I2CAddress_t address;
	I2CAddress_t reg_address;
	uint8_t index;
	bool reg_address_flag;
	bool rep_start;
} I2C_t;

// Inicializa el modulo I2C0
void i2c_drv_init(void);

// Inicia una TX/RX a un slave
I2CStatus_t i2c_drv_start_comm(uint8_t *data_arr, uint8_t size, I2CAddress_t address, I2CAddress_t reg_address,
							   I2CAction_t RW);

// Devuelve el estado actual del modulo I2C
I2CStatus_t i2c_get_status(void);

#endif
