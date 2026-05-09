#ifndef _SPI_H_
#define _SPI_H_
#include "port.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SPI_BUFF_SIZE 64U
#define SPI_DUMMY_BYTE 0xFFU

/*
**  MASTER SPI MODE!!
*/

/**
 * @brief Initialize an spi module.
 * @param spi_num spi num (0 to 2) @todo check this
 * @param baud_hz Baudrate in hz
 */
bool spi_drv_init(uint8_t spi_num, uint32_t baud);

/*
 * @brief Initialize a device in spi bus.
 * @returns The slave number, or -1 if an error ocurred
 */
uint8_t spi_drv_add_slave(uint8_t spi_num);

/**
 * @brief Queues to transfer buffer to send to slave. Non blocking
 * @param spi_num Spi module
 * @param slave_num Selected slave
 * @returns Number of bytes efectively queued into buffer. -1 on error
 */
uint8_t spi_drv_write(uint8_t spi_num, uint8_t slave_num, const uint8_t *tx_data, size_t len);

/**
 * @brief Queues to transfer buffer to send to slave. Non blocking
 * @param spi_num spi mod
 * @param slave_num Selected slave
 * @param rx_buf Reception buffer
 * @param len Desired len to receive. If there are less that desired len only the available bytes are received from buf
 * @note If there are less
 * @returns Number of bytes efectively read into buffer
 */
bool spi_drv_read(uint8_t spi_num, uint8_t slave_num, uint8_t *rx_buf, size_t len);


/**
* @brief Allows reception of data from slave 
* @param spi_num spi module
* @param slave_num Selected slave
**/
void spi_drv_allow_read(uint8_t spi_num, uint8_t slave_num);

/**
* @brief Stops reception of data from slave (incoming bytes are discarded)
* @param spi_num spi module
* @param slave_num Selected slave
**/
void spi_drv_notallow_read(uint8_t spi_num, uint8_t slave_num);


// @todo
// bool spi_drv_write_read(uint8_t spi_num, uint8_t slave_num, const uint8_t *tx_data, uint8_t *rx_buf, size_t len);

/**
 * @brief Number of free bytes in transmit buffer
 * @param spi_num The spi module number
 *
 */
uint8_t spi_drv_free_txt_buf(uint8_t spi_num);

/**
 * @brief Number of bytes available to read from RX software buffer + hw  RX FIFO.
 * @param spi_num The spi module number
 */
uint8_t spi_drv_free_rcv_buf(uint8_t spi_num);

#endif /* _SPI_H_ */
