#include "include/I2C.h"
#include "include/gpio.h"
#include "include/port.h"

#include <stdlib.h>

// Pines
#define I2C0_SCL_PIN 24
#define I2C0_SDA_PIN 25

// Frecuencias
#define I2C_BUS_CLOCK 50000000U
#define I2C_BAUD_RATE 100000U

// Mux
#define PORT_Alt 5

volatile static I2C_t I2C;							 // Objeto I2C
static I2C_Type *const I2C_ptr = I2C_BASE_PTRS;		 // Puntero al modulo I2C0
static PORT_Type *const PORT_ptr[] = PORT_BASE_PTRS; // Array de punteros a modulos PORT
static IRQn_Type const I2C_IRQn[] = I2C_IRQS;		 // Array de IRQs para los modulos I2C

uint8_t dummy;

void I2C_Init() {
	// Clock gating
	SIM->SCGC4 |= SIM_SCGC4_I2C0(HIGH);
	SIM->SCGC5 |= SIM_SCGC5_PORTE(HIGH);

	// Baudrate = 97.6k
	I2C_ptr->F = I2C_F_MULT(1) | I2C_F_ICR(0x2B);
	// Habilito modulo I2C y sus interrupciones
	I2C_ptr->C1 = I2C_C1_IICIE(HIGH) | I2C_C1_IICEN(HIGH);
	// Levanto flag de interrupcion
	I2C_ptr->S = I2C_S_IICIF(HIGH);

	// SCL mux a I2C y open drain
	PORT_ptr[PE]->PCR[I2C0_SCL_PIN] = LOW;
	PORT_ptr[PE]->PCR[I2C0_SCL_PIN] = PORT_PCR_MUX(PORT_Alt) | PORT_PCR_ODE(HIGH);

	// SDA mux a I2C y open drain
	PORT_ptr[PE]->PCR[I2C0_SDA_PIN] = LOW;
	PORT_ptr[PE]->PCR[I2C0_SDA_PIN] = PORT_PCR_MUX(PORT_Alt) | PORT_PCR_ODE(HIGH);

	NVIC_EnableIRQ(I2C_IRQn[0]);

	I2C.status = Done;
}

I2C_Status_t I2C_StartComm(uint8_t *data_arr, uint8_t size, I2C_Address_t address, I2C_Address_t reg_address,
						   I2C_RW_t RW) {
	if (data_arr != NULL && size) {
		I2C.status = Busy;
		I2C.address = address;
		I2C.reg_address = reg_address;
		I2C.reg_address_flag = false;
		I2C.data_arr = data_arr;
		I2C.size = size;
		I2C.index = 0;
		I2C.rep_start = false;
		I2C.RW = RW;
	}

	I2C.status = Busy;
	I2C_ptr->C1 |= I2C_C1_TX_MASK;	 // Arranco en Write
	I2C_ptr->C1 |= I2C_C1_MST_MASK;	 // Paso a modo master para mandar START
	I2C_ptr->D = (I2C.address) << 1; // Envio direccion

	return I2C.status;
}

I2C_Status_t I2C_GetStatus() {
	return I2C.status;
}

void I2C_IRQHandler() {
	static volatile uint8_t isr_count = 0;
	isr_count++;
	I2C_ptr->S |= I2C_S_IICIF_MASK; // Reseteo flag de interrupcion

	// Si estoy escribiendo
	if (I2C_ptr->C1 & I2C_C1_TX_MASK) {
		// Y estoy en modo write
		if (I2C.RW == Write) {
			// Si falta enviar datos
			if (I2C.index < I2C.size) {
				// Si llego el ACK
				if (!(I2C_ptr->S & I2C_S_RXAK_MASK)) {
					// Si ya envie la direccion del registro, mando el siguiente byte
					if (I2C.reg_address_flag) {
						I2C_ptr->D = I2C.data_arr[I2C.index];
						I2C.index++;
					}
					// Si no, mando la dir del registro
					else {
						I2C_ptr->D = I2C.reg_address;
						I2C.reg_address_flag = true;
					}
				}
				// Si hubo NACK
				else {
					I2C.status = Error;
					I2C_ptr->C1 &= ~I2C_C1_MST_MASK; // Paso a modo slave para mandar STOP
				}
			}
			// Si ya no quedan mas datos
			else if (I2C.index == I2C.size) {
				I2C.status = Done;
				I2C_ptr->C1 &= ~I2C_C1_MST_MASK; // Paso a modo slave para mandar STOP
			}
		}
		// Pero si estoy en modo read
		else if (I2C.RW == Read) {
			// Y recibi un ACK
			if (!(I2C_ptr->S & I2C_S_RXAK_MASK)) {
				// si no mande el reg_address
				if (!I2C.reg_address_flag) {
					I2C_ptr->D = I2C.reg_address;
					I2C.reg_address_flag = true;
				}
				// si ya mande el register address mando un repeated start
				else {
					// Si no mande el repeated start
					if (!I2C.rep_start) {
						I2C_ptr->C1 |= I2C_C1_RSTA_MASK;
						I2C_ptr->D = (I2C.address) << 1 | 0x00000001; // despues ponerle una macro
						I2C.rep_start = true;
					} else {
						I2C_ptr->C1 &= ~I2C_C1_TX_MASK;

						// Si hay un solo byte, mando NACK
						if (I2C.size == 1) {
							I2C_ptr->C1 |= I2C_C1_TXAK_MASK;
						} else
							I2C_ptr->C1 &= ~I2C_C1_TXAK_MASK;

						// Dummy read
						dummy = I2C_ptr->D;
					}
				}
			} else // No recibi ACK
			{
				I2C.status = Error;
				I2C_ptr->C1 &= ~I2C_C1_MST_MASK; // Paso a modo slave para mandar STOP;
			}
		}
	}
	// Si estoy leyendo
	else {
		// Si estoy en el ultimo byte
		if (I2C.index == I2C.size - 1) {
			I2C_ptr->C1 |= I2C_C1_TX_MASK;		  // Vuelvo a TX
			I2C.data_arr[I2C.index] = I2C_ptr->D; // Guardo el ultimo byte
			I2C_ptr->C1 &= ~I2C_C1_MST_MASK;	  // Paso a modo slave para mandar STOP;
			I2C.index++;
			I2C.status = Done;
		}
		// No estoy en el ultimo para leer
		else if (I2C.index < (I2C.size - 1)) {
			// Si le resto 2 me da el anteultimo
			if (I2C.index == (I2C.size - 2)) {
				I2C_ptr->C1 |= I2C_C1_TXAK_MASK; // Seteo NACK
			} else
				I2C_ptr->C1 &= ~I2C_C1_TXAK_MASK; // Seteo ACK
			I2C.data_arr[I2C.index] = I2C_ptr->D; // Guardo el byte
			I2C.index++;
		}
	}
}

__ISR__ I2C0_IRQHandler(void) {
	I2C_IRQHandler(0);
}
