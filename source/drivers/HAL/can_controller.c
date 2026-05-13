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

/*********** INTERRUPTS BITS enabled IN CANINTE (can interrupt enables) */
/*   7                                          0 */
/* MERRE WAKIE ERRIE TX2IE TX1IE TX0IE RX1IE RX0IE*/
#define MCP_INT_RX0 (1U << 0)
#define MCP_INT_RX1 (1U << 1)
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

// FORWARD DECS !
static bool reg_write_many(uint8_t spi_num, uint8_t slave_num, uint8_t addr, const uint8_t *data, uint8_t len);
static bool bit_modify(uint8_t spi_num, uint8_t slave_num, uint8_t addr, uint8_t mask, uint8_t data);
static bool reg_read(uint8_t spi_num, uint8_t slave_num, uint8_t addr, uint8_t *out);
static void wait_done(volatile bool *flag);

void can_gpio_irq(void);

static uint8_t slave_num;
static bool can_initialized = false;
static uint32_t uart_id;

static void delay_ms(uint32_t ms);
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
	// Use temp signed variable to properly catch -1 error return
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
	// delay_ms(20);

	wait_done(&done);

	/** Asuming 16MHz osc (?) -> datasheet confirms...
	 * CNF1:
	 *       SJW=00 (1 TQ fijo) , BRP=000111 (7 so TQ = 2×(7+1)/16MHz = 1us)
	 * CNF2:
	 *       BTLMODE=1 , SAM=0 , PHSEG1=110 (7 TQ) , PRSEG=101 (6 TQ)
	 * CNF3:
	 *       PHSEG2=001 (2 TQ)
	 */
	uint8_t timing[3] = {0x01U, 0xB5U, 0x07U};
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
	delay_ms(2); // let the frame clock out before returning

	return true;
}

/*****************************gpio interrupt*************************************/
void can_gpio_irq(void) {
	;
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

// poll until done with timeout (ms)
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

	// delay_ms(20);
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

	// delay_ms(20);
	wait_done(&done);

	uint8_t dummy[2];
	if (!spi_drv_read(spi_num, slave_num, dummy, sizeof(tx)))
		return false;

	return spi_drv_read(spi_num, slave_num, out, 1U);
}

// @todo take out
static void delay_ms(uint32_t ms) {
	volatile uint32_t cycles = ms * 15000U;
	while (cycles--)
		;
}