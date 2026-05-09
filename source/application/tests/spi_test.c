#include "include/spi_test.h"
#include "../../drivers/MCAL/include/spi.h"
#include "../../drivers/MCAL/include/uart.h"

#include <stdbool.h>
#include <string.h>

/*
 * SPI Driver Test
 *
 * IMPORTANT: we use transact because its a loopback
 *   spi_drv_write()    -> TX only
 *   spi_drv_transact() -> TX + RX capture
 * swtup:
 *      PTD3 (MOSI) <---> PTD2 (MISO)
 */

static uint8_t s_uart_id;
static uint8_t s_passed;
static uint8_t s_failed;

// @todo issue -> CS is still asserted when tests end!
/****************************** PRINT HELPERS *********************************/

static void _print(const char *str) {
	while (*str) {
		uint8_t c = (uint8_t) *str++;

		while (!UART_tstatus(s_uart_id))
			;

		UART_data_transmit(s_uart_id, &c, 1);
	}
}

static void _print_hex_byte(uint8_t val) {
	const char hex[] = "0123456789ABCDEF";

	uint8_t buf[4] = {'0', 'x', hex[val >> 4], hex[val & 0x0F]};

	UART_data_transmit(s_uart_id, buf, sizeof(buf));
}

static void _print_hex_buf(const uint8_t *buf, size_t len) {
	for (size_t i = 0; i < len; i++) {
		_print_hex_byte(buf[i]);
		_print(" ");
	}
}

static void _assert(bool condition, const char *name) {
	if (condition) {
		_print("  [PASS] ");
		s_passed++;
	} else {
		_print("  [FAIL] ");
		s_failed++;
	}

	_print(name);
	_print("\r\n");
}

static void delay_ms(uint32_t ms) {
	volatile uint32_t cycles = ms * 15000U;

	while (cycles--)
		;
}

static bool wait_done(volatile bool *done_flag, uint32_t timeout_ms) {
	while (timeout_ms--) {
		if (*done_flag)
			return true;

		delay_ms(1);
	}

	return false;
}

/****************************** TEST CASES ************************************/

static void test_init(void) {
	_print("\r\n-- Init --\r\n");

	bool ok = spi_drv_init(0, 1000000UL);

	_assert(ok, "spi_drv_init SPI0");

	bool double_init = spi_drv_init(0, 1000000UL);

	_assert(!double_init, "double init rejected");

	bool bad_num = spi_drv_init(3, 1000000UL);

	_assert(!bad_num, "invalid spi_num rejected");
}

static uint8_t test_add_slave(void) {
	_print("\r\n-- Add Slave --\r\n");

	uint8_t slave = spi_drv_add_slave(0);

	_assert(slave == 0, "first slave returns index 0");

	return slave;
}

static void test_buffers_idle(void) {
	_print("\r\n-- Buffer Status (idle) --\r\n");

	uint8_t tx_used = spi_drv_tx_busy(0);
	uint8_t rx_avail = spi_drv_rx_available(0);

	_assert(tx_used == 0, "TX reports 0b in use after init");

	_assert(rx_avail == 0, "RX reports 0b avail after init");
}

/*
 * TX ONLY TEST
 */
static void test_write_only(uint8_t slave) {
	_print("\r\n-- Write Only --\r\n");

	uint8_t tx[] = {0xAA, 0x55, 0xF0, 0x0F};

	volatile bool done = false;

	uint8_t queued = spi_drv_write(0, slave, tx, sizeof(tx), &done);

	_assert(queued == sizeof(tx), "write queued all bytes");

	bool completed = wait_done(&done, 50);

	_assert(completed, "write-only transfer completed");

	uint8_t busy = spi_drv_tx_busy(0);

	_assert(busy == 0, "tx_busy returns 0 after write");
}

/*
 * LOOPBACK TEST
 *
 * Uses transact() because this API only stores
 * RX data during transactions.
 */
static void test_transact_loopback(uint8_t slave) {
	_print("\r\n-- Loopback Transaction --\r\n");

	uint8_t tx[] = {0xA5, 0x3C, 0x55, 0xAA};

	uint8_t rx[4] = {0};

	volatile bool done = false;

	uint8_t queued = spi_drv_transact(0, slave, tx, sizeof(tx), sizeof(tx), &done);

	_assert(queued == sizeof(tx), "transact queued all TX bytes");

	bool completed = wait_done(&done, 50);

	_assert(completed, "transaction completed");

	uint8_t avail = spi_drv_rx_available(0);

	_assert(avail >= sizeof(rx), "RX bytes available after transaction");

	bool read_ok = spi_drv_read(0, slave, rx, sizeof(rx));

	_assert(read_ok, "read succeeded");

	bool match = (memcmp(tx, rx, sizeof(tx)) == 0);

	_assert(match, "loopback RX matches TX");

	if (!match) {
		_print("    TX: ");
		_print_hex_buf(tx, sizeof(tx));
		_print("\r\n");

		_print("    RX: ");
		_print_hex_buf(rx, sizeof(rx));
		_print("\r\n");
	}
}

static void test_read_insufficient(uint8_t slave) {
	_print("\r\n-- Insufficient Read --\r\n");

	uint8_t tx[] = {0x11, 0x22};

	uint8_t rx[4] = {0};

	volatile bool done = false;

	spi_drv_transact(0, slave, tx, sizeof(tx), sizeof(tx), &done);

	wait_done(&done, 50);

	bool too_many = spi_drv_read(0, slave, rx, 4);

	_assert(!too_many, "read rejected when requesting too much");

	bool exact = spi_drv_read(0, slave, rx, 2);

	_assert(exact, "read exact byte count succeeds");

	_assert(rx[0] == 0x11 && rx[1] == 0x22, "received bytes correct");
}

static void test_null_and_zero(uint8_t slave) {
	_print("\r\n-- Null / Zero Guards --\r\n");

	uint8_t dummy = 0xBB;

	volatile bool done = false;

	uint8_t q = spi_drv_write(0, slave, NULL, 4, &done);

	_assert(q == 0, "write with NULL data returns 0");

	uint8_t q2 = spi_drv_write(0, slave, &dummy, 0, &done);

	_assert(q2 == 0, "write with len=0 returns 0");

	bool r = spi_drv_read(0, slave, NULL, 4);

	_assert(!r, "read with NULL buffer returns false");

	bool r2 = spi_drv_read(0, slave, &dummy, 0);

	_assert(!r2, "read with len=0 returns false");
}

static void test_invalid_slave(void) {
	_print("\r\n-- Invalid Slave --\r\n");

	uint8_t tx[] = {0xBE, 0xEF};

	uint8_t rx[2];

	volatile bool done = false;

	uint8_t q = spi_drv_write(0, 5, tx, sizeof(tx), &done);

	_assert(q == 0, "write to invalid slave returns 0");

	bool r = spi_drv_read(0, 5, rx, sizeof(rx));

	_assert(!r, "read from invalid slave returns false");
}

/*
 * Stress test adapted for asynchronous driver.
 *
 * We do NOT assume the buffer remains fully occupied,
 * because ISR may drain concurrently.
 */
static void test_stress(uint8_t slave) {
	_print("\r\n-- Stress Transaction --\r\n");

	uint8_t tx[SPI_BUFF_SIZE];
	uint8_t rx[SPI_BUFF_SIZE];

	for (uint8_t i = 0; i < SPI_BUFF_SIZE; i++) {
		tx[i] = i;
		rx[i] = 0;
	}

	volatile bool done = false;

	uint8_t queued = spi_drv_transact(0, slave, tx, SPI_BUFF_SIZE, SPI_BUFF_SIZE, &done);

	_assert(queued == SPI_BUFF_SIZE, "full transaction queued");

	bool completed = wait_done(&done, 200);

	_assert(completed, "stress transaction completed");

	uint8_t avail = spi_drv_rx_available(0);

	_assert(avail >= SPI_BUFF_SIZE, "all RX bytes available");

	bool read_ok = spi_drv_read(0, slave, rx, SPI_BUFF_SIZE);

	_assert(read_ok, "stress read succeeded");

	bool match = (memcmp(tx, rx, SPI_BUFF_SIZE) == 0);

	_assert(match, "stress loopback matches");

	if (!match) {
		_print("    TX: ");
		_print_hex_buf(tx, SPI_BUFF_SIZE);
		_print("\r\n");

		_print("    RX: ");
		_print_hex_buf(rx, SPI_BUFF_SIZE);
		_print("\r\n");
	}
}

/****************************** MAIN ******************************************/

void spi_test_app(uint8_t uart_id) {
	s_uart_id = uart_id;

	_print("\r\n==============================\r\n");
	_print("   SPI Driver Test\r\n");
	_print("==============================\r\n");

	test_init();

	uint8_t slave = test_add_slave();

	test_buffers_idle();

	test_write_only(slave);

	test_transact_loopback(slave);

	test_read_insufficient(slave);

	test_null_and_zero(slave);

	test_invalid_slave();

	test_stress(slave);

	_print("\r\nPassed: ");
	_print_hex_byte(s_passed);

	_print("  Failed: ");
	_print_hex_byte(s_failed);

	_print("\r\n");
}