#include "include/can_controller.h"
#include "../MCAL/include/gpio.h"
#include "../MCAL/include/spi.h"
#include "../MCAL/include/uart.h"

#include "include/board.h"
#include <string.h>

/***********************************+*/
#define CAN_MSG_ID 0x101U
/** SETUP ********************************************************* */
#define CAN_SPI_NUM 0
#define CAN_SPI_BAUDRATE 1000000UL
#define CAN_CS_NUM 0

/* SPI INSTRUCTIONS********************+*********************************/
#define MCP_RESET 0xC0U /* puts can in CONFIG mode */
#define MCP_READ 0x03U
#define MCP_WRITE 0x02U		  /*  for more than one auto increment addresses */
#define MCP_BIT_MODIFY 0x05U  /* Modify individual bits in a register !!*/
#define MCP_READ_STATUS 0xA0U /* fast read of common status bits */
#define MCP_RX_STATUS 0xB0U	  /* Fast-read of receive status */

#define MCP_RTS_TX0 0x81U /*TX  buffer 0 */
#define MCP_RTS_TX1 0x82U /*  TX buffer 1 */
#define MCP_RTS_TX2 0x84U /* TX buffer 2 */

/** REGISTER ADDRESSES1!! *********************************************** */
/* bit timing regs */
#define MCP_REG_CNF3 0x28U
#define MCP_REG_CNF2 0x29U
#define MCP_REG_CNF1 0x2AU

/* Interrupt enable / flag */
#define MCP_REG_CANINTE 0x2BU
#define MCP_REG_CANINTF 0x2CU

/* Error flag */
#define MCP_REG_EFLG 0x2DU

/* CAN control & status */
#define MCP_REG_CANCTRL 0x0FU
#define MCP_REG_CANSTAT 0x0EU

/* RX buffer controls */
#define MCP_REG_RXB0CTRL 0x60U
#define MCP_REG_RXB1CTRL 0x70U

/* TX buffer controls */
#define MCP_REG_TXB0CTRL 0x30U
#define MCP_REG_TXB1CTRL 0x40U
#define MCP_REG_TXB2CTRL 0x50U

// @todo add the other buffers
#define MCP_REG_TXB0SIDH 0x31U
#define MCP_REG_RXB0SIDH 0x61U
#define MCP_REG_RXB1SIDH 0x71U

/*********** INTERRUPTS BITS enabled IN CANINTE (can interrupt enables) */
/*   7                                          0 */
/* MERRE WAKIE ERRIE TX2IE TX1IE TX0IE RX1IE RX0IE*/
#define MCP_INT_RX0 (1U << 0)
#define MCP_INT_RX1 (1U << 1)
#define MCP_INT_TX0 (1U << 2) /* enabled only while a TX occcurring */
#define MCP_INT_ERR (1U << 5)
#define MCP_INT_MERR (1U << 7)

/**************** filter reception register control */
#define MCP_RXB_RXM_MASK 0x60U
#define MCP_RXB_RXM_ANY 0x60U
#define MCP_RXB_RXM_FILTER 0x00U /*  filters/masks */

/******** REQOP mask and bits */
#define MCP_CANCTRL_REQOP_MASK 0xE0U
#define MCP_MODE_NORMAL 0x00U
#define MCP_MODE_CONFIG 0x80U

#define MCP_TXREQ (1U << 3)
#define MCP_TXERR (1U << 4)

/**********************TRANSMISSION BUFFER REGISTERS */
#define TX_BUFF_COUNT 3
#define TX_DATA_BYTES 8U

typedef struct {
	uint8_t TXBCtrlReg;
	uint8_t TXBSIDHReg;
	uint8_t TXBSIDLReg; /* we dont use EID0,EID8 */
	uint8_t TXBEID0Reg;
	uint8_t TXBEID8Reg;
	uint8_t TXBDLCReg;
	uint8_t TXBDataReg[TX_DATA_BYTES];
} TxRegsBufMM_t;
static TxRegsBufMM_t tx_registers[TX_BUFF_COUNT] = {
	{0x30U, 0x31U, 0x32U, 0x33U, 0x34U, 0x35U, {0x36U, 0x37U, 0x38U, 0x39U, 0x3AU, 0x3BU, 0x3CU, 0x3DU}},
	{0x40U, 0x41U, 0x42U, 0x43U, 0x44U, 0x45U, {0x46U, 0x47U, 0x48U, 0x49U, 0x4AU, 0x4BU, 0x4CU, 0x4DU}},
	{0x50U, 0x51U, 0x52U, 0x53U, 0x54U, 0x55U, {0x56U, 0x57U, 0x58U, 0x59U, 0x5AU, 0x5BU, 0x5CU, 0x5DU}}};

/*
 * RX FRAME: SIDH SIDL EID8 EID0 DLC D0..D7
 **/
#define RX_FRAME_HEADER 5U
#define RX_FRAME_MAX (RX_FRAME_HEADER + 8U)

typedef enum {
	TX_IDLE,
	TX_WAIT_FRAME_WRITE,  /* writing SIDH..data over SPI */
	TX_WAIT_INTE_ENABLE,  /* enabling TX0IE */
	TX_WAIT_RTS,		  /* RTS command clocked out */
	TX_WAIT_BUS,		  /* frame on bus, waiting for INT  (TX0IF) */
	TX_WAIT_CTRL_READ,	  /* reading TXB0CTRL to check TXERR */
	TX_WAIT_INTE_DISABLE, /* disabling TX0IE */
	TX_WAIT_INTF_CLEAR,	  /* clearing TX0IF */
} TXState_t;

/* =========================================================================
 * IRQ / RX STATE MACHINE
 * ========================================================================= */
typedef enum {
	IRQ_IDLE,
	IRQ_WAIT_INTF_READ,
	IRQ_WAIT_RX0_READ,
	IRQ_WAIT_RX1_READ,
	IRQ_WAIT_INTF_CLEAR,
} IRQState_t;

static uint8_t slave_num;
static bool can_initialized = false;

/* spi falg !*/
static volatile bool spi_done = false;

/* tx */
static TXState_t tx_state = TX_IDLE;
static void (*tx_done_cb)(bool) = NULL;
static uint8_t tx_frame[13];
static uint8_t tx_ctrl_val;
static bool tx_ok;

/* irq/rx */
static IRQState_t irq_state = IRQ_IDLE;
static volatile bool irq_pending = false;
static uint8_t irq_intf_val;
static uint8_t rx_buf[RX_FRAME_MAX];
static void (*rx_cb)(const uint8_t *data, uint8_t dlc) = NULL;

/*  SPI temp buffers */
static uint8_t spi_tx_buf[SPI_BUFF_SIZE];
static uint8_t spi_rx_dummy[2];

// FORWARD DECS !
static bool reg_write_many(uint8_t spi_num, uint8_t slave_num, uint8_t addr, const uint8_t *data, uint8_t len);
static bool bit_modify(uint8_t spi_num, uint8_t slave_num, uint8_t addr, uint8_t mask, uint8_t data);
static bool reg_read(uint8_t spi_num, uint8_t slave_num, uint8_t addr, uint8_t *out);
static void wait_done(volatile bool *flag);

static bool start_reg_write(uint8_t addr, const uint8_t *data, uint8_t len);
static bool start_bit_modify(uint8_t addr, uint8_t mask, uint8_t data);
static bool start_reg_read(uint8_t addr);
static bool spi_write_buf(uint8_t *buf, uint8_t len);
static bool spi_transact_buf(uint8_t *tx, uint8_t tx_len, uint8_t rx_total);

/* state machine processors */
static void process_tx(void);
static void process_irq(void);
static void finish_rx_frame(uint8_t bufidx);

void can_gpio_irq(void);

static uint32_t uart_id;

/**
 * @brief Initialize the can controller driver. Configures interruptiion pin and comm (spi0)
 * @note COMPLETELY BLOCKING AND NEEDS INTERRUPTS ENABLED
 */
bool can_controller_drv_init() {
	// Only initialize once
	if (can_initialized) {
		return true;
	}

	uart_id = UART_drv_instance_init(PIN_UART0_RX, PIN_UART0_TX, UART0_BAUDRATE);

	// initialize spi
	if (!spi_drv_init(CAN_SPI_NUM, CAN_SPI_BAUDRATE)) {
		return false;
	}

	int8_t temp_slave = spi_drv_add_slave(CAN_SPI_NUM);
	if (temp_slave < 0) {
		return false;
	}
	slave_num = (uint8_t) temp_slave; // Now assign valid value

	// set up can registers !

	// first we must enter configuration mode:
	/**
	 * The MCP25625 must be initialized before activation.
	 *  This is only possible if the device is in Configuration mode.
	 * Configuration mode is automatically selected after power- up, a Reset ....
	 * Configuration mode is the only mode where the following registers are modifiable: • CNF1, CNF2, CNF3 ,
	 * TXRTSCTRL, filters
	 */
	uint8_t rst = MCP_RESET;
	volatile bool done = false;
	if (spi_drv_write(CAN_SPI_NUM, slave_num, &rst, 1U, &done) == 0U)
		return false;

	wait_done(&done);

	/** Asuming 16MHz osc (?) -> datasheet confirms...
	 * THIS IS WRONG !!!
	 * CNF1:
	 *       SJW=00 (1 TQ fijo) , BRP=000111 (7 so TQ = 2×(7+1)/16MHz = 1us)
	 * CNF2:
	 *       BTLMODE=1 , SAM=0 , PHSEG1=110 (7 TQ) , PRSEG=101 (6 TQ)
	 * CNF3:
	 *       PHSEG2=001 (2 TQ)
	 */
	/* CORRECT IS:
		/**
	 * 16 MHz oscillator, 125 kbps, 8 TQ total:
	 *   TQ  = 2*(BRP+1)/OSC = 2*(7+1)/16MHz = 1 us
	 *   Seg = SyncSeg(1) + PRSEG(1) + PHSEG1(3) + PHSEG2(3) = 8 TQ -> 1/8us = 125kbps
	 *   timing[0]-> CNF3 = 0x02  (PHSEG2 = 010 -> 3 TQ)
	 *   timing[1] -> CNF2 = 0x90  (BTLMODE=1, PHSEG1=010, PRSEG=000)
	 *   timing[2] -> CNF1 = 0x07  (SJW=00, BRP=000111)
	 */

	// uint8_t timing[3] = {0x01U, 0xB5U, 0x07U};
	uint8_t timing[3] = {0x02U, 0x90U, 0x07U};
	if (!reg_write_many(CAN_SPI_NUM, slave_num, MCP_REG_CNF3, timing, 3U))
		return false;

	// Enable interrupts @todo remember discard error packages in irq!
	// only enabling in receive buffer and error
	uint8_t inte = MCP_INT_RX0 | MCP_INT_RX1 | MCP_INT_ERR | MCP_INT_MERR;
	if (!reg_write_many(CAN_SPI_NUM, slave_num, MCP_REG_CANINTE, &inte, 1U)) // len 1 is single
		return false;

	// configure reception buffers
	/**
	 * According to datasheet:
	 * RXM<1:0>: Receive Buffer Operating mode bits
			11 = Turns mask/filters off; receives any message @todo add id filtering, requires using extended filter
	 regs!
	 */
	/*
	uint8_t rxb0 = MCP_RXB_RXM_ANY | 0x04U; // bit2 is rollover: BUKT
	if (!reg_write_many(CAN_SPI_NUM, slave_num, MCP_REG_RXB0CTRL, &rxb0, 1U))
		return false;
	uint8_t rxb1 = MCP_RXB_RXM_ANY;
	if (!reg_write_many(CAN_SPI_NUM, slave_num, MCP_REG_RXB1CTRL, &rxb1, 1U))
		return false;
		*/

	if (!bit_modify(CAN_SPI_NUM, slave_num, MCP_REG_RXB0CTRL, MCP_RXB_RXM_MASK, MCP_RXB_RXM_ANY))
		return false;
	if (!bit_modify(CAN_SPI_NUM, slave_num, MCP_REG_RXB1CTRL, MCP_RXB_RXM_MASK, MCP_RXB_RXM_ANY))
		return false;

	// "START" -> normal operation, modify register bits directyly
	if (!bit_modify(CAN_SPI_NUM, slave_num, MCP_REG_CANCTRL, MCP_CANCTRL_REQOP_MASK, (uint8_t) MCP_MODE_NORMAL))
		return false;

	// CHECK!
	uint8_t stat = 0U;
	if (!reg_read(CAN_SPI_NUM, slave_num, MCP_REG_CANSTAT, &stat) ||
		(stat & MCP_CANCTRL_REQOP_MASK) != (uint8_t) MCP_MODE_NORMAL) {
		return false;
	}
	UART_data_transmit(uart_id, "Stat:", 6); // @todo take out
	UART_data_transmit(uart_id, &stat, 1);	 // @todo take out
	can_initialized = true;

	// set up interrupt gpio !
	/**
	 * according to MCP25625:
	 * When an interrupt occurs,
	 * the INT pin is driven low by the MCP25625
	 * and will remain low until the interrupt is cleared by the MCU.
	 * An interrupt can not be cleared if the respective condition still prevails.
	 */
	gpio_drv_mode(PIN_CAN_INT, INPUT_PULLUP); //
	if (!gpio_drv_IRQ(PIN_CAN_INT, GPIO_IRQ_MODE_FALLING_EDGE, can_gpio_irq)) {
		return false;
	}
	UART_data_transmit(uart_id, (uint8_t *) "can setup done", 15); // @todo take out
	return true;
}

/**
bool can_send(const uint8_t *data, uint8_t len) {
	if (len > 8U)
		return false;

	uint8_t frame[13] = {0};						  // SIDH, SIDL, EID8, EID0, DLC, D0..D7
	frame[0] = (uint8_t) (CAN_MSG_ID >> 3);			  // SIDH: ID[10:3]
	frame[1] = (uint8_t) ((CAN_MSG_ID & 0x07U) << 5); // SIDL: ID[2:0] in bits[7:5], EXIDE=0
	frame[2] = 0x00U;								  // EID8 unused
	frame[3] = 0x00U;								  // EID0 unused
	frame[4] = len & 0x0FU;							  // DLC, RTR=0
	memcpy(&frame[5], data, len);

	// write frame registers
	if (!reg_write_many(CAN_SPI_NUM, slave_num, tx_registers[0].TXBSIDHReg, frame, 5U + len))
		return false;

	// request transmission via RTS
	volatile bool done = false;
	uint8_t rts = MCP_RTS_TX0;
	if (spi_drv_write(CAN_SPI_NUM, slave_num, &rts, 1U, &done) == 0U)
		return false;
	wait_done(&done);

	return true;
}
	*/
/**
 * @brief Register callback for received CAN frames
 */
void can_set_rx_cb(void (*cb)(const uint8_t *data, uint8_t dlc)) {
	rx_cb = cb;
}

/**
 * @brief Queue a CAN frame for transmission (non-blocking).
 * @return false if driver not ready or a TX is already in progress
 */
bool can_send(const uint8_t *data, uint8_t len, void (*on_done)(bool success)) {
	if (!can_initialized)
		return false;
	if (tx_state != TX_IDLE)
		return false;
	if (len > 8U)
		return false;

	tx_done_cb = on_done;

	tx_frame[0] = (uint8_t) (CAN_MSG_ID >> 3);
	tx_frame[1] = (uint8_t) ((CAN_MSG_ID & 0x07U) << 5);
	tx_frame[2] = 0x00U;
	tx_frame[3] = 0x00U;
	tx_frame[4] = len & 0x0FU;
	memcpy(&tx_frame[5], data, len);

	if (!start_reg_write(MCP_REG_TXB0SIDH, tx_frame, 5U + len))
		return false;

	tx_state = TX_WAIT_FRAME_WRITE;
	return true;
}

/**
 * @brief Drive TX and RX state machines. Must be called from main loop!
 */
void can_process(void) {
	/* TX  priority over IRQ */
	if (tx_state != TX_IDLE && tx_state != TX_WAIT_BUS) {
		process_tx();
		return;
	}

	/* Start IRQ handling only when SPI is free */
	if (irq_pending && tx_state == TX_IDLE && irq_state == IRQ_IDLE) {
		irq_pending = false;
		if (start_reg_read(MCP_REG_CANINTF))
			irq_state = IRQ_WAIT_INTF_READ;
	}

	if (irq_state != IRQ_IDLE) {
		process_irq();
	}
}

/*****************************gpio interrupt*************************************/

/**
 * @brief GPIO ISR — MCP25625 INT pin fell low.
 *        Sets flag only, work is done in can_process().
 */
void can_gpio_irq(void) {
	irq_pending = true;
}

static void process_tx(void) {
	if (!spi_done)
		return;
	spi_done = false;

	switch (tx_state) {
		case TX_WAIT_FRAME_WRITE:
			/* enable TX0IE so INT fires when bus TX completes. */
			if (!start_bit_modify(MCP_REG_CANINTE, MCP_INT_TX0, MCP_INT_TX0))
				goto tx_error;
			tx_state = TX_WAIT_INTE_ENABLE;
			break;

		case TX_WAIT_INTE_ENABLE:
			spi_tx_buf[0] = MCP_RTS_TX0;
			if (!spi_write_buf(spi_tx_buf, 1U))
				goto tx_error;
			tx_state = TX_WAIT_RTS;
			break;

		case TX_WAIT_RTS:
			/* RTS is on the wire. Park here until can_gpio_irq fires TX0IF,
			 * which process_irq will detect and forward to TX_WAIT_CTRL_READ. */
			tx_state = TX_WAIT_BUS;
			break;

		case TX_WAIT_CTRL_READ:
			/* Returned here by process_irq after TX0IF detected */
			if (!spi_drv_read(CAN_SPI_NUM, slave_num, spi_rx_dummy, 2U))
				goto tx_error;
			if (!spi_drv_read(CAN_SPI_NUM, slave_num, &tx_ctrl_val, 1U))
				goto tx_error;
			tx_ok = !(tx_ctrl_val & MCP_TXERR);

			if (!start_bit_modify(MCP_REG_CANINTE, MCP_INT_TX0, 0x00U))
				goto tx_error;
			tx_state = TX_WAIT_INTE_DISABLE;
			break;

		case TX_WAIT_INTE_DISABLE:
			if (!start_bit_modify(MCP_REG_CANINTF, MCP_INT_TX0, 0x00U))
				goto tx_error;
			tx_state = TX_WAIT_INTF_CLEAR;
			break;

		case TX_WAIT_INTF_CLEAR: {
			bool result = tx_ok;
			tx_state = TX_IDLE;
			if (tx_done_cb)
				tx_done_cb(result);
			break;
		}

		default:
			break;
	}
	return;

tx_error:
	start_bit_modify(MCP_REG_CANINTE, MCP_INT_TX0, 0x00U); /* best-effort */
	tx_state = TX_IDLE;
	if (tx_done_cb)
		tx_done_cb(false);
}

static void process_irq(void) {
	if (!spi_done)
		return;
	spi_done = false;

	switch (irq_state) {
		case IRQ_WAIT_INTF_READ: {
			if (!spi_drv_read(CAN_SPI_NUM, slave_num, spi_rx_dummy, 2U)) {
				irq_state = IRQ_IDLE;
				return;
			}
			if (!spi_drv_read(CAN_SPI_NUM, slave_num, &irq_intf_val, 1U)) {
				irq_state = IRQ_IDLE;
				return;
			}

			/* TX0IF: forward to TX state machine to read CTRL and clean up */
			if (irq_intf_val & MCP_INT_TX0) {
				tx_state = TX_WAIT_CTRL_READ;
				if (!start_reg_read(MCP_REG_TXB0CTRL)) {
					tx_state = TX_IDLE;
					if (tx_done_cb)
						tx_done_cb(false);
				}
				irq_intf_val &= (uint8_t) ~MCP_INT_TX0;
				/* TX machine clears TX0IF itself; fall through to handle any RX */
			}

			if (irq_intf_val & MCP_INT_RX0) {
				if (!start_reg_read(MCP_REG_RXB0SIDH)) {
					irq_state = IRQ_IDLE;
					return;
				}
				irq_state = IRQ_WAIT_RX0_READ;
				return;
			}

			if (irq_intf_val & MCP_INT_RX1) {
				if (!start_reg_read(MCP_REG_RXB1SIDH)) {
					irq_state = IRQ_IDLE;
					return;
				}
				irq_state = IRQ_WAIT_RX1_READ;
				return;
			}

			if (irq_intf_val & (MCP_INT_ERR | MCP_INT_MERR)) {
				/* @todo read EFLG for diagnostics before clearing */
				if (!start_bit_modify(MCP_REG_CANINTF, MCP_INT_ERR | MCP_INT_MERR, 0x00U)) {
					irq_state = IRQ_IDLE;
					return;
				}
				irq_state = IRQ_WAIT_INTF_CLEAR;
				return;
			}

			irq_state = IRQ_IDLE;
			break;
		}

		case IRQ_WAIT_RX0_READ:
			finish_rx_frame(0U);
			break;
		case IRQ_WAIT_RX1_READ:
			finish_rx_frame(1U);
			break;

		case IRQ_WAIT_INTF_CLEAR:
			irq_state = IRQ_IDLE;
			break;

		default:
			irq_state = IRQ_IDLE;
			break;
	}
}

static void finish_rx_frame(uint8_t bufidx) {
	if (!spi_drv_read(CAN_SPI_NUM, slave_num, spi_rx_dummy, 2U)) {
		irq_state = IRQ_IDLE;
		return;
	}
	if (!spi_drv_read(CAN_SPI_NUM, slave_num, rx_buf, RX_FRAME_MAX)) {
		irq_state = IRQ_IDLE;
		return;
	}

	uint8_t dlc = rx_buf[4] & 0x0FU;
	if (dlc > 8U)
		dlc = 8U;
	if (rx_cb)
		rx_cb(&rx_buf[5], dlc);

	uint8_t flag = (bufidx == 0U) ? MCP_INT_RX0 : MCP_INT_RX1;
	if (!start_bit_modify(MCP_REG_CANINTF, flag, 0x00U)) {
		irq_state = IRQ_IDLE;
		return;
	}
	irq_state = IRQ_WAIT_INTF_CLEAR;
}

/*
 * NON-BLOCKING SPI HELPERS
 *  spi_done set by SPI ISR
 */
static bool spi_write_buf(uint8_t *buf, uint8_t len) {
	spi_done = false;
	return spi_drv_write(CAN_SPI_NUM, slave_num, buf, len, (bool *) &spi_done) != 0U;
}

static bool spi_transact_buf(uint8_t *tx, uint8_t tx_len, uint8_t rx_total) {
	spi_done = false;
	return spi_drv_transact(CAN_SPI_NUM, slave_num, tx, tx_len, rx_total, (bool *) &spi_done) != 0U;
}

static bool start_reg_write(uint8_t addr, const uint8_t *data, uint8_t len) {
	if ((uint8_t) (2U + len) > SPI_BUFF_SIZE)
		return false;
	spi_tx_buf[0] = MCP_WRITE;
	spi_tx_buf[1] = addr;
	memcpy(&spi_tx_buf[2], data, len);
	return spi_write_buf(spi_tx_buf, (uint8_t) (2U + len));
}

static bool start_bit_modify(uint8_t addr, uint8_t mask, uint8_t data) {
	spi_tx_buf[0] = MCP_BIT_MODIFY;
	spi_tx_buf[1] = addr;
	spi_tx_buf[2] = mask;
	spi_tx_buf[3] = data;
	return spi_write_buf(spi_tx_buf, 4U);
}

/**
 * Requests 2 (cmd echo) + RX_FRAME_MAX rx bytes
 * reads and full RX frame reads with a single function
 */
static bool start_reg_read(uint8_t addr) {
	spi_tx_buf[0] = MCP_READ;
	spi_tx_buf[1] = addr;
	return spi_transact_buf(spi_tx_buf, 2U, (uint8_t) (2U + RX_FRAME_MAX));
}

/**************************************HELPERS***********************************/
// first address, all data in buf !!, write "len" registers with data data
static bool reg_write_many(uint8_t spi_num, uint8_t slave_num, uint8_t addr, const uint8_t *data, uint8_t len) {
	if ((uint8_t) (2U + len) > SPI_BUFF_SIZE)
		return false;

	uint8_t buf[SPI_BUFF_SIZE];
	buf[0] = MCP_WRITE; // write regs!
	buf[1] = addr;
	memcpy(&buf[2], data, len);

	volatile bool done = false;
	uint8_t queued = spi_drv_write(spi_num, slave_num, buf, (size_t) (2U + len), &done);
	if (queued == 0U)
		return false;
	wait_done(&done);
	return true;
}

// poll until done
static inline void wait_done(volatile bool *flag) {
	while (!(*flag))
		;
}

/**
 * BIT_MODIFY instruction
 * Only available for registers: CNF* , CANINTE, CANINTF, control registers, filters, some others
 * frame is [MCP_BIT_MODIFY | addr | mask | data]
 */
static bool bit_modify(uint8_t spi_num, uint8_t slave_num, uint8_t addr, uint8_t mask, uint8_t data) {
	uint8_t buf[4] = {MCP_BIT_MODIFY, addr, mask, data};
	volatile bool done = false;
	uint8_t queued = spi_drv_write(spi_num, slave_num, buf, sizeof(buf), &done);
	if (queued == 0U)
		return false;

	wait_done(&done);
	return true;
}

/*
 * Total clocked bytes = tx_len + rx_len = 2 + 1 = 3
 * we discard first two dummy bytes, reading the 1 real byte
 */
static bool reg_read(uint8_t spi_num, uint8_t slave_num, uint8_t addr, uint8_t *out) {
	uint8_t tx[2] = {MCP_READ, addr};
	volatile bool done = false;

	uint8_t total_rx = (uint8_t) (sizeof(tx) + 1U);
	uint8_t queued = spi_drv_transact(spi_num, slave_num, tx, sizeof(tx), total_rx, &done);
	if (queued == 0U)
		return false;

	wait_done(&done);

	uint8_t dummy[2];
	if (!spi_drv_read(spi_num, slave_num, dummy, sizeof(tx)))
		return false;

	return spi_drv_read(spi_num, slave_num, out, 1U);
}
