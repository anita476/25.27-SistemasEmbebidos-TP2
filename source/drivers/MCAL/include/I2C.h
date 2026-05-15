#ifndef _I2C_H_
#define _I2C_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum{
	Read,
	Write,
}I2C_RW_t;

typedef enum{
  Busy,	        	// Currently transmitting or receiving
  Done,		      	// Already finished communication
  Error			    // Some error occurred
} I2C_Status_t;

typedef uint8_t I2C_Address_t;

typedef struct{
	volatile I2C_Status_t status;
	I2C_RW_t RW;
	uint8_t * data_arr;
	uint8_t size;
	I2C_Address_t address;
  	I2C_Address_t reg_address;
	uint8_t index;
	bool reg_address_flag;
	bool rep_start;
}I2C_t;

//Inicializa el modulo I2C0
void I2C_Init(void);

//Inicia una TX/RX a un slave
I2C_Status_t I2C_StartComm(uint8_t * data_arr, uint8_t size, I2C_Address_t address, I2C_Address_t reg_address, I2C_RW_t RW);

//Devuelve el estado actual del modulo I2C
I2C_Status_t I2C_GetStatus(void);

#endif
