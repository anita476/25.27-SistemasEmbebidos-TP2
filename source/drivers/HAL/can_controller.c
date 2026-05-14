/******************************************************************************
 * OBS!!!
	thiis is a blocking implementation !!!!
 ******************************************************************************/

#include "include/can_controller.h"
#include "../MCAL/include/gpio.h"
#include "../MCAL/include/spi.h"
#include "include/board.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** SETUP ********************************************************* */
#define CAN_MSG_ID 0x101U
#define CAN_SPI_NUM 0
#define CAN_SPI_BAUDRATE 1000000UL
#define CAN_CS_NUM 0

/******************************************************************************
 * MCP25625 COMMANDS
 ******************************************************************************/
#define MCP_RESET 0xC0U
#define MCP_READ 0x03U
#define MCP_WRITE 0x02U
#define MCP_BIT_MODIFY 0x05U
#define MCP_RTS_TX0 0x81U

/* *****************************************************************************
 * REGISTERS
 ******************************************************************************/

#define MCP_REG_CANSTAT 0x0EU
#define MCP_REG_CANCTRL 0x0FU

#define MCP_REG_CNF3 0x28U
#define MCP_REG_CNF2 0x29U
#define MCP_REG_CNF1 0x2AU

#define MCP_REG_CANINTE 0x2BU
#define MCP_REG_CANINTF 0x2CU

#define MCP_REG_TXB0CTRL 0x30U
#define MCP_REG_TXB0SIDH 0x31U
#define MCP_REG_TXB0SIDL 0x32U
#define MCP_REG_TXB0DLC 0x35U
#define MCP_REG_TXB0DATA 0x36U

#define MCP_REG_RXB0CTRL 0x60U
#define MCP_REG_RXB0SIDH 0x61U
#define MCP_REG_RXB0SIDL 0x62U

#define MCP_REG_RXB0DLC 0x65U
#define MCP_REG_RXB0DATA 0x66U

/* filters*/
#define MCP_REG_RXF0SIDH 0x00U
#define MCP_REG_RXF0SIDL 0x01U

#define MCP_REG_RXM0SIDH 0x20U
#define MCP_REG_RXM0SIDL 0x21U

/******************************************************************************
 * BIT MASKS !
 ******************************************************************************/

#define MCP_MODE_NORMAL 0x00U
#define MCP_CANCTRL_REQOP_MASK 0xE0U
#define MCP_INT_RX0 0x01U
#define MCP_INT_RX1 0x02U
#define MCP_INT_TX0 0x04U
#define MCP_INT_TX1 0x08U
#define MCP_INT_TX2 0x10U
#define MCP_INT_ERR 0x20U
#define MCP_RXM_MASK 0x60U
#define MCP_RXM_ANY 0x60U

#define CAN_RX_QUEUE_SIZE 8U
typedef struct {
	CanFrame_t buffer[CAN_RX_QUEUE_SIZE];
	volatile uint8_t head;
	volatile uint8_t tail;
} CanRXQueue_t;

static CanRXQueue_t rx_queue = {.head = 0, .tail = 0};

static bool can_initialized = false;
static volatile bool can_irq_pending = false;
static volatile bool tx_busy = false;

static uint8_t slave_num = 0;
static can_tx_cb_t tx_cb = NULL;

// we keep ONE BUFFER for spi operations throughout driver -> otherways may get discarded with locla context and
// hardfault occurs
static uint8_t spi_tx_buf[16];
static uint8_t spi_rx_buf[16];

/******************************************************************************
 * FORWARD DECS
 ******************************************************************************/
static void can_gpio_irq(void);
static bool spi_write_blocking(const uint8_t *tx, size_t len);
static bool spi_transact_blocking(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len);
static bool reg_read(uint8_t reg, uint8_t *out);
static bool reg_write(uint8_t reg, const uint8_t *data, uint8_t len);
static bool bit_modify(uint8_t reg, uint8_t mask, uint8_t data);
static void service_interrupts(uint8_t intf);
static void service_tx0(void);
static void service_rx0(void);
static uint8_t rx_next(uint8_t idx);
static bool rx_queue_empty(void);
static bool rx_queue_full(void);
static bool rx_queue_push(const CanFrame_t *frame);
static bool rx_queue_pop(CanFrame_t *frame);
static void service_rx0(void);

/********* contract  */
bool can_controller_drv_init(void) {
	if (can_initialized)
		return true;

	if (!spi_drv_init(CAN_SPI_NUM, CAN_SPI_BAUDRATE)) {
		return false;
	}

	int8_t tmp_slave = spi_drv_add_slave(CAN_SPI_NUM);
	if (tmp_slave < 0)
		return false;
	slave_num = (uint8_t) tmp_slave;

	// set up can registers !

	// first we must enter configuration mode:
	/**
	 * The MCP25625 must be initialized before activation.
	 *  This is only possible if the device is in Configuration mode.
	 * Configuration mode is automatically selected after power- up, a Reset ....
	 * Configuration mode is the only mode where the following registers are modifiable: • CNF1, CNF2, CNF3 ,
	 * TXRTSCTRL, filters
	 */
	spi_tx_buf[0] = MCP_RESET;

	if (!spi_write_blocking(spi_tx_buf, 1))
		return false;

	/** Asuming 16MHz osc (?) -> datasheet confirms...
	 * CORRECT IS:
	 * 16 MHz oscillator, 125 kbps, 8 TQ total:
	 *   TQ  = 2*(BRP+1)/OSC = 2*(7+1)/16MHz = 1 us
	 *   Seg = SyncSeg(1) + PRSEG(1) + PHSEG1(3) + PHSEG2(3) = 8 TQ -> 1/8us = 125kbps
	 *   timing[0]-> CNF3 = 0x02  (PHSEG2 = 010 -> 3 TQ)
	 *   timing[1] -> CNF2 = 0x90  (BTLMODE=1, PHSEG1=010, PRSEG=000)
	 *   timing[2] -> CNF1 = 0x07  (SJW=00, BRP=000111)
	 */

	// uint8_t timing[3] = {0x01U, 0xB5U, 0x07U};
	uint8_t cnf3 = 0x02U;
	uint8_t cnf2 = 0x90U;
	uint8_t cnf1 = 0x07U;

	// @todo may leter reimplment write many...
	if (!reg_write(MCP_REG_CNF3, &cnf3, 1))
		return false;

	if (!reg_write(MCP_REG_CNF2, &cnf2, 1))
		return false;

	if (!reg_write(MCP_REG_CNF1, &cnf1, 1))
		return false;

	/** @todo implement filters  */

	// if (!bit_modify(MCP_REG_RXB0CTRL, MCP_RXM_MASK, MCP_RXM_ANY)) {
	//	return false;
	// }

	/**
	 * ACCEPT only IDs 0x100 - 0x107
	 * Ids go from 0000 0001 0000 0000
	 * 			to 0000 0001 0000 0111
	 * So mask is:
	 *			        0111 1111 1000 -> 0x7F8
	 * And filter is :
	 * 					0001 0000 0000 -> 0x100
	 */

	/*
	 * RXM = 00 -> use filters
	 */
	if (!bit_modify(MCP_REG_RXB0CTRL, MCP_RXM_MASK, 0x00U)) {
		return false;
	}

	/*
	 * RX MASK 0 = 0x7F8
	 * SIDH = ID[10:3]
	 * SIDL = ID[2:0] << 5
	 */

	uint16_t mask = 0x7F8U;

	uint8_t mask_sidh = (uint8_t) (mask >> 3);
	uint8_t mask_sidl = (uint8_t) ((mask & 0x07U) << 5);

	if (!reg_write(MCP_REG_RXM0SIDH, &mask_sidh, 1)) {
		return false;
	}

	if (!reg_write(MCP_REG_RXM0SIDL, &mask_sidl, 1)) {
		return false;
	}

	/*
	 * FILTER 0 = 0x100
	 */

	uint16_t filter = 0x100U;

	uint8_t filter_sidh = (uint8_t) (filter >> 3);
	uint8_t filter_sidl = (uint8_t) ((filter & 0x07U) << 5);

	if (!reg_write(MCP_REG_RXF0SIDH, &filter_sidh, 1)) {
		return false;
	}

	if (!reg_write(MCP_REG_RXF0SIDL, &filter_sidl, 1)) {
		return false;
	}

	// @todo maybe add error handling later
	uint8_t inte = MCP_INT_TX0 | MCP_INT_RX0;

	if (!reg_write(MCP_REG_CANINTE, &inte, 1)) {
		return false;
	}

	// modify can control to enter normal op mode
	if (!bit_modify(MCP_REG_CANCTRL, MCP_CANCTRL_REQOP_MASK, MCP_MODE_NORMAL)) {
		return false;
	}

	// CHECK!
	uint8_t stat = 0;
	if (!reg_read(MCP_REG_CANSTAT, &stat)) {
		return false;
	}
	if ((stat & MCP_CANCTRL_REQOP_MASK) != MCP_MODE_NORMAL) {
		return false;
	}

	// set up interrupt gpio !
	/**
	 * according to MCP25625:
	 * When an interrupt occurs,
	 * the INT pin is driven low by the MCP25625
	 * and will remain low until the interrupt is cleared by the MCU.
	 * An interrupt can not be cleared if the respective condition still prevails.
	 */
	// @todo better than edge detection would be with LOW/HIGH level
	gpio_drv_mode(PIN_CAN_INT, INPUT_PULLUP);
	if (!gpio_drv_IRQ(PIN_CAN_INT, GPIO_IRQ_MODE_FALLING_EDGE, can_gpio_irq)) {
		return false;
	}

	can_initialized = true;
	return true;
}

void can_process(void) {
	if (!can_initialized)
		return;

	if (!can_irq_pending)
		return;

	can_irq_pending = false;

	uint8_t intf = 0;

	if (!reg_read(MCP_REG_CANINTF, &intf)) {
		return;
	}

	service_interrupts(intf);

	/*
	 * IMPORTANT:
	 *
	 * INT stays LOW while ANY interrupt remains pending.
	 *
	 * Since GPIO only triggers on FALLING EDGE,
	 * we must manually retrigger processing.
	 */
	if (!gpio_drv_read(PIN_CAN_INT)) {
		can_irq_pending = true;
	}
}

bool can_send(const uint8_t *data, uint8_t len, can_tx_cb_t cb) {
	if (!can_initialized)
		return false;

	if (tx_busy)
		return false;

	if (len > 8)
		return false;

	tx_busy = true;

	tx_cb = cb;

	uint8_t sidh = (uint8_t) (CAN_MSG_ID >> 3);
	uint8_t sidl = (uint8_t) ((CAN_MSG_ID & 0x07U) << 5);

	if (!reg_write(MCP_REG_TXB0SIDH, &sidh, 1)) {
		tx_busy = false;
		return false;
	}
	if (!reg_write(MCP_REG_TXB0SIDL, &sidl, 1)) {
		tx_busy = false;
		return false;
	}
	uint8_t dlc = len;
	if (!reg_write(MCP_REG_TXB0DLC, &dlc, 1)) {
		tx_busy = false;
		return false;
	}
	if (!reg_write(MCP_REG_TXB0DATA, data, len)) {
		tx_busy = false;
		return false;
	}
	spi_tx_buf[0] = MCP_RTS_TX0;
	if (!spi_write_blocking(spi_tx_buf, 1)) {
		tx_busy = false;
		return false;
	}
	return true;
}
// can we write ?
bool can_available(void) {
	return !rx_queue_empty();
}
// read a frame from can buff !
bool can_read(CanFrame_t *frame) {
	if (frame == NULL)
		return false;

	return rx_queue_pop(frame);
}

/**
 * @brief GPIO ISR — MCP25625 INT pin fell low.
 *        Sets flag only, work is done in can_process().
 */
void can_gpio_irq(void) {
	can_irq_pending = true;
}

/********* HELPERS  *********/

static void service_interrupts(uint8_t intf) {
	if (intf & MCP_INT_TX0)
		service_tx0();

	if (intf & MCP_INT_RX0)
		service_rx0();
}

// tx is complet, clear flag
static void service_tx0(void) {
	bit_modify(MCP_REG_CANINTF, MCP_INT_TX0, 0x00U);

	tx_busy = false;
	if (tx_cb != NULL) {
		tx_cb(true);
	}
}

bool get_int_blocking(uint8_t *out) {
	return reg_read(MCP_REG_CANINTF, out);
}
static bool reg_read(uint8_t reg, uint8_t *out) {
	spi_tx_buf[0] = MCP_READ;
	spi_tx_buf[1] = reg;

	if (!spi_transact_blocking(spi_tx_buf, 2, spi_rx_buf, 3)) {
		return false;
	}

	*out = spi_rx_buf[2];

	return true;
}

static bool reg_write(uint8_t reg, const uint8_t *data, uint8_t len) {
	spi_tx_buf[0] = MCP_WRITE;
	spi_tx_buf[1] = reg;

	memcpy(&spi_tx_buf[2], data, len);

	return spi_write_blocking(spi_tx_buf, len + 2);
}

static bool bit_modify(uint8_t reg, uint8_t mask, uint8_t data) {
	spi_tx_buf[0] = MCP_BIT_MODIFY;
	spi_tx_buf[1] = reg;
	spi_tx_buf[2] = mask;
	spi_tx_buf[3] = data;

	return spi_write_blocking(spi_tx_buf, 4);
}

static bool spi_write_blocking(const uint8_t *tx, size_t len) {
	static volatile bool done;

	done = false;

	if (!spi_drv_write(CAN_SPI_NUM, slave_num, tx, len, &done)) {
		return false;
	}

	while (!done) {
	}

	return true;
}

static bool spi_transact_blocking(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len) {
	static volatile bool done;
	done = false;

	if (!spi_drv_transact(CAN_SPI_NUM, slave_num, tx, tx_len, rx_len, &done)) {
		return false;
	}
	while (!done) {
	}
	return spi_drv_read(CAN_SPI_NUM, slave_num, rx, rx_len);
}

static uint8_t rx_next(uint8_t idx) {
	return (uint8_t) ((idx + 1U) % CAN_RX_QUEUE_SIZE);
}

static bool rx_queue_empty(void) {
	return rx_queue.head == rx_queue.tail;
}

static bool rx_queue_full(void) {
	return rx_next(rx_queue.head) == rx_queue.tail;
}

static bool rx_queue_push(const CanFrame_t *frame) {
	if (rx_queue_full())
		return false;

	rx_queue.buffer[rx_queue.head] = *frame;
	rx_queue.head = rx_next(rx_queue.head);

	return true;
}

static bool rx_queue_pop(CanFrame_t *frame) {
	if (rx_queue_empty())
		return false;

	*frame = rx_queue.buffer[rx_queue.tail];
	rx_queue.tail = rx_next(rx_queue.tail);
	return true;
}

static void service_rx0(void) {
	CanFrame_t frame;
	uint8_t sidh = 0;
	uint8_t sidl = 0;
	uint8_t dlc = 0;

	if (!reg_read(MCP_REG_RXB0SIDH, &sidh))
		return;
	if (!reg_read(MCP_REG_RXB0SIDL, &sidl))
		return;

	frame.id = ((uint32_t) sidh << 3) | (sidl >> 5);
	if (!reg_read(MCP_REG_RXB0DLC, &dlc))
		return;
	frame.dlc = dlc & 0x0FU;
	if (frame.dlc > 8U)
		frame.dlc = 8U;
	for (uint8_t i = 0; i < frame.dlc; i++) {
		if (!reg_read((uint8_t) (MCP_REG_RXB0DATA + i), &frame.data[i])) {
			return;
		}
	}
	rx_queue_push(&frame);
	// clear flag !!
	bit_modify(MCP_REG_CANINTF, MCP_INT_RX0, 0x00U);
}