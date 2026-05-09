#include "include/spi_test.h"
#include "../../drivers/MCAL/include/spi.h"
#include "../../drivers/MCAL/include/uart.h"

/*
 * Test application for SPI driver
 *
 *   UART0 inited
 *   SPI0 loopback: PTD3 (MOSI) <---jumper---> PTD2 (MISO)
 *   SPI0 slave 0: PCS0 on PTD0
 */

static uint8_t s_uart_id;
static uint8_t s_passed;
static uint8_t s_failed;

/****************************** PRINT HELPERS *********************************/

static void _print(const char *str) {
	while (*str) {
		uint8_t c = (uint8_t) *str++;
		while (!UART_tstatus(s_uart_id))
			; // wait for space
		UART_data_transmit(s_uart_id, &c, 1);
	}
}

static void _print_hex_byte(uint8_t val) {
	const char hex[] = "0123456789ABCDEF";
	uint8_t buf[4] = {'0', 'x', hex[val >> 4], hex[val & 0xF]};
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

	int8_t slave = spi_drv_add_slave(0);
	_assert(slave == 0, "first slave returns index 0");

	spi_drv_allow_read(0, slave); // always allow reading for loopback test!

	int8_t bad = spi_drv_add_slave(2); // SPI2 not initialized
	_assert(bad == -1, "add_slave on uninit spi rejected");

	return (uint8_t) slave;
}

static void test_free_buffers_empty(void) {
	_print("\r\n-- Buffer Status (idle) --\r\n");

	uint8_t tx_used = spi_drv_free_txt_buf(0);
	uint8_t rx_avail = spi_drv_free_rcv_buf(0);

	_assert(tx_used == 0, "TX reports 0b in use after init");
	_assert(rx_avail == 0, "RX reports 0b avail after init");

	// invalid module returns 0
	_assert(spi_drv_free_txt_buf(3) == 0, "free_txt_buf invalid module returns 0");
	_assert(spi_drv_free_rcv_buf(3) == 0, "free_rcv_buf invalid module returns 0");
}

static void test_write_loopback(uint8_t slave) {
	_print("\r\n-- Loopback Write/Read --\r\n");

	uint8_t tx[] = {0xA5, 0x3C, 0x55, 0xAA};
	uint8_t rx[4] = {0};

	uint8_t queued = spi_drv_write(0, slave, tx, sizeof(tx));
	_assert(queued == sizeof(tx), "write queued all bytes");

	delay_ms(5);

	bool read_ok = spi_drv_read(0, slave, rx, sizeof(rx));
	_assert(read_ok, "read succeeded after loopback delay");

	bool match = (memcmp(tx, rx, sizeof(tx)) == 0);
	_assert(match, "loopback data matches TX");

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

	spi_drv_write(0, slave, tx, sizeof(tx));
	delay_ms(5);

	// request more than available
	bool too_many = spi_drv_read(0, slave, rx, 4);
	_assert(!too_many, "read rejected when requesting more than available");

	// read exactly what was sent
	bool exact = spi_drv_read(0, slave, rx, 2);
	_assert(exact, "read succeeded for exact available count");
	_assert(rx[0] == 0x11 && rx[1] == 0x22, "partial read data correct");
}

static void test_null_and_zero(uint8_t slave) {
	_print("\r\n-- Null / Zero Guards --\r\n");

	uint8_t dummy = 0xBB;

	uint8_t q = spi_drv_write(0, slave, NULL, 4);
	_assert(q == 0, "write with NULL data returns 0");

	uint8_t q2 = spi_drv_write(0, slave, &dummy, 0);
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

	uint8_t q = spi_drv_write(0, 5, tx, sizeof(tx)); // slave 5 never added
	_assert(q == 0, "write to invalid slave returns 0");

	bool r = spi_drv_read(0, 5, rx, sizeof(rx));
	_assert(!r, "read from invalid slave returns false");
}

static void test_stress(uint8_t slave) {
	_print("\r\n-- Stress: Full Buffer --\r\n");

	uint8_t tx[SPI_BUFF_SIZE];
	uint8_t rx[SPI_BUFF_SIZE] = {0};

	for (uint8_t i = 0; i < SPI_BUFF_SIZE; i++)
		tx[i] = i;

	uint8_t queued = spi_drv_write(0, slave, tx, SPI_BUFF_SIZE);
	_assert(queued == SPI_BUFF_SIZE, "full buffer write accepted");

	// overflow: one more byte while buffer is full
	uint8_t extra = 0xFF;
	uint8_t overflow = spi_drv_write(0, slave, &extra, 1);
	_assert(overflow == 0, "write rejected when buffer full");

	delay_ms(15);

	bool read_ok = spi_drv_read(0, slave, rx, SPI_BUFF_SIZE);
	_assert(read_ok, "full buffer loopback read succeeded");

	bool match = (memcmp(tx, rx, SPI_BUFF_SIZE) == 0);
	_assert(match, "stress loopback data matches");

	if (!match) {
		_print("    TX: ");
		_print_hex_buf(tx, SPI_BUFF_SIZE);
		_print("\r\n");
		_print("    RX: ");
		_print_hex_buf(rx, SPI_BUFF_SIZE);
		_print("\r\n");
	}
}

static void test_free_buffers_after_tx(uint8_t slave) {
	_print("\r\n-- Buffer Counters After TX --\r\n");

	uint8_t tx[] = {0x01, 0x02, 0x03, 0x04};
	spi_drv_write(0, slave, tx, sizeof(tx));

	// immediately after write, SW+HW should show bytes in flight
	uint8_t in_flight = spi_drv_free_txt_buf(0);
	_assert(in_flight > 0, "free_txt_buf shows bytes in use after write");

	delay_ms(5);

	// after transfer completes, TX should drain
	uint8_t after = spi_drv_free_txt_buf(0);
	_assert(after == 0, "free_txt_buf shows 0 after transfer completes");

	// RX should now have bytes available
	uint8_t rx_avail = spi_drv_free_rcv_buf(0);
	_assert(rx_avail == sizeof(tx), "free_rcv_buf shows received byte count");

	// drain RX so next test starts clean
	uint8_t discard[4];
	spi_drv_read(0, slave, discard, sizeof(tx));
}

/****************************** MAIN ******************************************/

void spi_test_app(uint8_t uart_id) {
	s_uart_id = uart_id;
	_print("\r\n==============================\r\n");
	_print("   SPI Driver Loopback Test \r\n");
	_print("==============================\r\n");

	test_init();

	uint8_t slave = test_add_slave();

	test_free_buffers_empty();
	test_write_loopback(slave);
	test_read_insufficient(slave);
	test_null_and_zero(slave);
	test_invalid_slave();
	test_stress(slave);
	test_free_buffers_after_tx(slave);

	_print("Passed: ");
	_print_hex_byte(s_passed);
	_print("  Failed: ");
	_print_hex_byte(s_failed);
}