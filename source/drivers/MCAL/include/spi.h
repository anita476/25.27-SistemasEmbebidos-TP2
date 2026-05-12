#ifndef _SPI_H_
#define _SPI_H_
#include "port.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SPI_BUFF_SIZE 255U
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
 * @brief Initialize a device in spi bus. Slaves supported depends on module.
 * @returns The slave number, or -1 if an error ocurred.
 */
int8_t spi_drv_add_slave(uint8_t spi_num);

/**
 * @brief TX only: Queues to transfer buffer to send to slave. Non blocking. CS held low for entire len bytes via CONT.
 * @param spi_num Spi module
 * @param slave_num Selected slave
 * @param done_flag Flag that signals write over
 * @returns Number of bytes efectively queued into buffer. 0 on error
 */
uint8_t spi_drv_write(uint8_t spi_num, uint8_t slave_num, const uint8_t *tx_data, size_t len, volatile bool *done_flag);

/*
 * @brief Send instruction bytes then clock in response bytes, CS held throughout.
		  Non-blocking since ISR sets *done_flag when all rx_len bytes are stored
 *        Retrieve data afterwards with spi_drv_read()!!
 * @returns bytes queued into TX buffer, 0 on error.
 * @note OBs!!! Está hecho así así funciona para loopback + para cuando la transmisión es realmente full duplex.
 * 		 EVENTUALLY ADD FLAG TO ONLY SAVE TRUE RX TO BUFF!!
 */
uint8_t spi_drv_transact(uint8_t spi_num, uint8_t slave_num, const uint8_t *tx_data, size_t tx_len, size_t rx_len,
						 volatile bool *done_flag);

/*
 * @brief Copy len bytes from RX SW buffer into rx_buf.
 *        Call after done_flag is set. Falls back to draining HW FIFO if needed.
 * @returns true if len bytes were available and copied.
 */
bool spi_drv_read(uint8_t spi_num, uint8_t slave_num, uint8_t *rx_buf, size_t len);
/**
 * @brief Number of bytes in use in tx buffer
 *
 */
uint8_t spi_drv_tx_busy(uint8_t spi_num);

/* @brief Bytes available to read (SW RX buffer + HW RX FIFO). */
uint8_t spi_drv_rx_available(uint8_t spi_num);
#endif /* _SPI_H_ */
