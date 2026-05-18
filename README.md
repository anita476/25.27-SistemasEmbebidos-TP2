# Sistemas Embebidos - TP2
## Grupo 1


Integrantes: 
* Ana Negre
* Rodrigo Devesa 
* Cristóbal Kramer 
* Federico Gentile

### Descripción General

Este proyecto implementa un sistema de comunicación CAN para una placa FRDM-K64F, permitiendo la transmisión y recepción de datos a través de un bus CAN usando el transciever+controller MCP25625.


### Protocolos de Comunicación 

#### Protocolo Serial
El protocolo de comunicación serial está implementado en `protocol_handler.py` y define el siguiente formato de mensajes:

- **Frame de Ángulos**: `XYZ[R|C|O]VVVV\r\n`
  - `XYZ`: ID CAN en formato hexadecimal (3 dígitos ASCII)
  - `[R|C|O]`: Id del tipo de ángulo (Roll, Cabeceo, Orientación)
  - `VVVV`: Valor numérico del ángulo

- **Frame de LED Remoto**: `'L<byte>\r\n'`
  - Comando de LED para otras estaciones (solo se registra en log)


### Test Pins

Se incluyen pines específicos para verificar la funcionalidad de las interrupciones:

* `TP` `PTB2`, para interrupciones Systick
* `UART_TP` `PTB3`, SPI_TP `PTB10` para interrupciones de periféricos UART y SPI.
* `GPIO_TP` para interrupciones dedicadas de puerto/gpio (incluye la interrupción de bus CAN e I2C)


### TiltNetworkTool

La carpeta `TiltNetworkTool-1.0.0-macOSfix` contiene una versión modificada de TiltNetwork para funcionar en macOS. 

**Nota**: Las modificaciones realizadas son específicamente para compatibilidad con macOS. El protocolo de comunicación requerido está definido en `protocol_handler.py`.

