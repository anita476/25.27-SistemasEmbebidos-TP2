#ifndef _FXOS_H_
#define _FXOS_H_

#include <stdint.h>
#include <stdbool.h>


typedef struct{
    int16_t x;
    int16_t y;
    int16_t z;
}SRAWDATA;

typedef enum{
  FXOS_OK,	        	// Datos recibidos
  FXOS_ERROR,		    // Ocurrio algun error
} FXOS_Status_t;

//Inicializa el modulo I2C y configura los registros del FXOS
int FXOS_Init(void);

//Toma mediciones del acelerometro y magnetometro del FXOS. Devuelve vectores en X,Y,Z para cada uno.
FXOS_Status_t ReadAccelMagnData(SRAWDATA *pAccelData, SRAWDATA *pMagnData);

#endif
