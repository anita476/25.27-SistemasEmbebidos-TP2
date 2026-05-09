#include "include/spi.h"
#include "include/gpio.h"
#include "include/port.h"
#include <stdint.h>

#define SPI_HAL_DEFAULT_BAUDRATE 1000000UL // 1 MHz
#define SPI_COUNT 3
#define SPI_MAX_DEVICE_COUNT 6 // not all are implemented in all spi modules


typedef struct {
	pin_t pcs;
	PORTMux_t alt;
} Device_t;

typedef struct {
	uint8_t spi_num;
	PORT port;
	pin_t miso; // sin
	pin_t mosi; // sout
	pin_t sck;
	Device_t pcsn[SPI_MAX_DEVICE_COUNT]; // ss signals
	PORTMux_t alt;
} SPIConfig_t;

typedef struct {
	uint8_t buf[SPI_BUFF_SIZE];
	volatile uint8_t head;
	volatile uint8_t tail;
	volatile uint8_t count; // in use!
} SPIRingBuff_t;

typedef struct {
	bool active;
	uint8_t slave_count;
	uint8_t current_slave; // slave being transmitted to
	SPIRingBuff_t tx_buf;
	SPIRingBuff_t rx_buf;
	volatile bool tx_pending; // do we have something to send?
	volatile bool rx_pending; // do we want to receive?
} SPIState_t;

static SPI_Type *const spi_ptrs[] = SPI_BASE_PTRS;
static SPIState_t spi_state[SPI_COUNT];
static SPIConfig_t const spi_pin_map[SPI_COUNT] = {{0,
													PD,
													PORTNUM2PIN(PD, 3),
													PORTNUM2PIN(PD, 2), // not using the one on uart0
													PORTNUM2PIN(PD, 1),
													{{PORTNUM2PIN(PD, 0), PORT_mAlt2},
													 {PORTNUM2PIN(PD, 4), PORT_mAlt2},
													 {PORTNUM2PIN(PD, 5), PORT_mAlt2},
													 {PORTNUM2PIN(PD, 6), PORT_mAlt2},
													 {PORTNUM2PIN(PC, 0), PORT_mAlt2},
													 {PORTNUM2PIN(PC, 23), PORT_mAlt3}},
													PORT_mAlt2},
												   {
													   1,
													   PE,
													   PORTNUM2PIN(PE, 3),
													   PORTNUM2PIN(PE, 1),
													   PORTNUM2PIN(PE, 2),
													   {{PORTNUM2PIN(PE, 4), PORT_mAlt2},
														{PORTNUM2PIN(PE, 0), PORT_mAlt2},
														{PORTNUM2PIN(PE, 5), PORT_mAlt2},
														{PORTNUM2PIN(PE, 6), PORT_mAlt2},
														{0, 0 /*dont care*/},
														{0, 0 /*dont care*/}},
													   PORT_mAlt2,
												   },
												   {2,
													PB,
													PORTNUM2PIN(PB, 23),
													PORTNUM2PIN(PB, 22),
													PORTNUM2PIN(PB, 21),
													{{PORTNUM2PIN(PB, 20), PORT_mAlt2},
													 {0, 0 /*dont care*/},
													 {0, 0 /*dont care*/},
													 {0, 0 /*dont care*/},
													 {0, 0 /*dont care*/},
													 {0, 0 /*dont care*/}},
													PORT_mAlt2}};

// Lookup for clock gating and nvic ints
uint32_t const spi_sim_masks[SPI_COUNT] = {SIM_SCGC6_SPI0_MASK, SIM_SCGC6_SPI1_MASK, SIM_SCGC3_SPI2_MASK};

uint32_t const spi_nvic_ints[SPI_COUNT] = {SPI0_IRQn, SPI1_IRQn, SPI2_IRQn};

static void _spi_drv_set_baudrate(uint8_t spi_num, uint32_t baud);
static uint8_t _spi_get_next_device(uint8_t spi_num, uint8_t slave_num);
static bool _buf_push(SPIRingBuff_t *buf, uint8_t byte);
static bool _buf_pop(SPIRingBuff_t *buf, uint8_t *byte);
static void _spi_drain_rx_fifo(uint8_t spi_num);

/********************************* FUNCTIONS***********************************/

bool spi_drv_init(uint8_t spi_num, uint32_t baud) {
	// check spi num
	if (spi_num >= SPI_COUNT || spi_state[spi_num].active) {
		return false;
	}
	/// clock gating
	if (spi_num == 2) {
		SIM->SCGC3 |= spi_sim_masks[spi_num];
	} else {
		SIM->SCGC6 |= spi_sim_masks[spi_num];
	}

	// port
	SIM->SCGC5 |= port_clock_masks[spi_pin_map[spi_num].port]; // we assume all pins on same port except pcs

	// pins alt
	port_ptrs[spi_pin_map[spi_num].port]->PCR[PIN2NUM(spi_pin_map[spi_num].miso)] =
		PORT_PCR_MUX(spi_pin_map[spi_num].alt) | PORT_PCR_IRQC(PORT_eDisabled);

	//
	port_ptrs[spi_pin_map[spi_num].port]->PCR[PIN2NUM(spi_pin_map[spi_num].miso)] =
		PORT_PCR_MUX(spi_pin_map[spi_num].alt) | PORT_PCR_IRQC(PORT_eDisabled);

	port_ptrs[spi_pin_map[spi_num].port]->PCR[PIN2NUM(spi_pin_map[spi_num].sck)] =
		PORT_PCR_MUX(spi_pin_map[spi_num].alt) | PORT_PCR_IRQC(PORT_eDisabled);

	// halt module before configuration
	spi_ptrs[spi_num]->MCR = SPI_MCR_MSTR_MASK								// master mode
							 | SPI_MCR_PCSIS_MASK							// PCS active low!
							 | SPI_MCR_HALT_MASK							// halt
							 | SPI_MCR_CLR_TXF_MASK | SPI_MCR_CLR_RXF_MASK; // clear  FIFOs

	// frame format
	spi_ptrs[spi_num]->CTAR[0] = SPI_CTAR_FMSZ(7) // 8-bit frames
								 | SPI_CTAR_CPOL(0) |
								 SPI_CTAR_CPHA(0); // default polarity and phase @todo mayb chang later

	_spi_drv_set_baudrate(spi_num, baud);

	// enable RX drain interrupt
	// rser is DMA/Interrupt Request Select and Enable Register,
	// MODULE CANT BE RUNNING TO EDIT
	spi_ptrs[spi_num]->RSER = SPI_RSER_RFDF_RE_MASK; // RX FIFO drain request enable

	// clear status (32bits)
	spi_ptrs[spi_num]->SR = 0xFFFFFFFFU;

	// enable NVICs
	NVIC_ClearPendingIRQ(spi_nvic_ints[spi_num]);
	NVIC_EnableIRQ(spi_nvic_ints[spi_num]);

	// release halt, module is now active
	spi_ptrs[spi_num]->MCR &= ~SPI_MCR_HALT_MASK;

	spi_state[spi_num].active = true;
	spi_state[spi_num].slave_count = 0;
	spi_state[spi_num].current_slave = 0;
	spi_state[spi_num].tx_buf = (SPIRingBuff_t) {0};
	spi_state[spi_num].rx_buf = (SPIRingBuff_t) {0};

	return true;
}

/*
 * @brief Initialize a device in spi bus. Slaves supported depends on module.
 * @returns The slave number, or -1 if an error ocurred.
 */
uint8_t spi_drv_add_slave(uint8_t spi_num) {
	if (spi_num >= SPI_COUNT || !spi_state[spi_num].active) {
		return -1;
	}
	if (spi_state[spi_num].slave_count >= SPI_MAX_DEVICE_COUNT) {
		return -1;
	}

	uint8_t slave_num = spi_state[spi_num].slave_count;

	// get next available PCS pin from the map
	uint8_t pcs_pin = _spi_get_next_device(spi_num, slave_num);
	if (pcs_pin == 0) {
		return -1;
	}
	uint8_t pcs_port = PIN2PORT(pcs_pin);
	uint8_t pcs_alt = spi_pin_map[spi_num].pcsn[slave_num].alt;

	// enable port clock
	SIM->SCGC5 |= port_clock_masks[pcs_port];

	port_ptrs[pcs_port]->PCR[PIN2NUM(pcs_pin)] = PORT_PCR_MUX(pcs_alt) | PORT_PCR_IRQC(PORT_eDisabled);

	spi_state[spi_num].slave_count++;
	return (int8_t) slave_num;
}

/**
 * @brief Queues to transfer buffer to send to slave. Non blocking
 * @param spi_num Spi module
 * @param slave_num Selected slave
 * @returns Number of bytes efectively queued into buffer. -1 on error
 */
uint8_t spi_drv_write(uint8_t spi_num, uint8_t slave_num, const uint8_t *tx_data, size_t len) {
	if (spi_num >= SPI_COUNT || !spi_state[spi_num].active)
		return 0;
	if (slave_num >= spi_state[spi_num].slave_count || tx_data == NULL || len == 0)
		return 0;

	spi_state[spi_num].current_slave = slave_num;

	uint8_t queued = 0;
	for (size_t i = 0; i < len; i++) {
		if (!_buf_push(&spi_state[spi_num].tx_buf, tx_data[i]))
			break;
		queued++;
	}
	if (queued > 0) {
		spi_state[spi_num].tx_pending = true;
		// rx_pending stays whatever it was — don't touch it
		spi_ptrs[spi_num]->RSER |= SPI_RSER_TFFF_RE_MASK;
	}
	return queued;
}

/**
 * @brief Queues to transfer buffer to send to slave. Non blocking
 * @param spi_num spi mod
 * @param slave_num Selected slave
 * @param rx_buf Reception buffer
 * @param len Desired len to receive. If there are less that desired len only the available bytes are received from
 * buf
 * @note If there are less
 * @returns Number of bytes efectively read into buffer
 */
bool spi_drv_read(uint8_t spi_num, uint8_t slave_num, uint8_t *rx_buf, size_t len) {
	if (spi_num >= SPI_COUNT || !spi_state[spi_num].active)
		return false;
	if (slave_num >= spi_state[spi_num].slave_count || rx_buf == NULL || len == 0)
		return false;

	SPIState_t *st = &spi_state[spi_num];

	// not enough in SW buffer , try to pull from HW FIFO before giving up
	if (st->rx_buf.count < len) {
		_spi_drain_rx_fifo(spi_num);
	}

	// check again after draining
	if (st->rx_buf.count < len) {
		return false;
	}

	for (size_t i = 0; i < len; i++) {
		_buf_pop(&st->rx_buf, &rx_buf[i]);
	}
	return true;
}

// @todo
// bool spi_drv_write_read(uint8_t spi_num, uint8_t slave_num, const uint8_t *tx_data, uint8_t *rx_buf, size_t len);

/**
 * @brief Number of free bytes available in tx buffer
 *
 */
uint8_t spi_drv_free_txt_buf(uint8_t spi_num) {
	if (spi_num >= SPI_COUNT || !spi_state[spi_num].active) {
		return 0;
	}
	// TXCTR holds number of entriesin tx fifo
	uint8_t hw_used = (uint8_t) ((spi_ptrs[spi_num]->SR & SPI_SR_TXCTR_MASK) >> SPI_SR_TXCTR_SHIFT);
	uint8_t sw_used = spi_state[spi_num].tx_buf.count;
	return (sw_used) + (hw_used); // free SW slots ++ free HW FIFO slots
}

/**
 * @brief Number of bytes available to read from RX software buffer + hw  RX FIFO.
 */
uint8_t spi_drv_free_rcv_buf(uint8_t spi_num) {
	if (spi_num >= SPI_COUNT || !spi_state[spi_num].active) {
		return 0;
	}
	// RXCTR holds number of entries in rx fifo
	uint8_t hw_avail = (uint8_t) ((spi_ptrs[spi_num]->SR & SPI_SR_RXCTR_MASK) >> SPI_SR_RXCTR_SHIFT);
	uint8_t sw_avail = spi_state[spi_num].rx_buf.count;
	return sw_avail + hw_avail; // total bytes readable right now
}

/**
* @brief Allows reception of data from slave 
* @param spi_num spi module
* @param slave_num Selected slave
**/
void spi_drv_allow_read(uint8_t spi_num, uint8_t slave_num){
	if(spi_num >= SPI_COUNT || !spi_state[spi_num].active|| !(spi_state[spi_num].slave_count > slave_num)){return;}
	spi_state[spi_num].rx_pending = true;
}

/**
* @brief Stops reception of data from slave (incoming bytes are discarded)
* @param spi_num spi module
* @param slave_num Selected slave
**/
void spi_drv_notallow_read(uint8_t spi_num, uint8_t slave_num){
	if(spi_num >= SPI_COUNT || !spi_state[spi_num].active|| !(spi_state[spi_num].slave_count > slave_num)){return;}
	spi_state[spi_num].rx_pending =false;

}

/*****************************************INTERRUPTS  ROUTINES******************************************/

static void _spi_irq_handler(uint8_t spi_num) {
	SPI_Type *spi = spi_ptrs[spi_num];
	uint32_t sr = spi->SR;
	SPIState_t *st = &spi_state[spi_num];

	// RX
	if (sr & SPI_SR_RFDF_MASK) {
		spi->SR = SPI_SR_RFDF_MASK; // clear w1c FIRST
		while (spi->SR & SPI_SR_RXCTR_MASK) {
			uint8_t byte = (uint8_t) (spi->POPR);
			if (st->rx_pending) {
				_buf_push(&st->rx_buf, byte); // store if read was requested
			}
			// discard
		}
	}
	// TX
	if (sr & SPI_SR_TFFF_MASK) {
		spi->SR = SPI_SR_TFFF_MASK; // clear w1c FIRST
		uint8_t byte;
		while (((spi->SR & SPI_SR_TXCTR_MASK) >> SPI_SR_TXCTR_SHIFT) < 4) {
			if (st->tx_pending && _buf_pop(&st->tx_buf, &byte)) {
				// real data to send
				spi->PUSHR = SPI_PUSHR_PCS(1U << st->current_slave) | SPI_PUSHR_TXDATA(byte);
			} else if (st->rx_pending && st->rx_buf.count < SPI_BUFF_SIZE) {
				// otherwise push dummy to generate clock
				spi->PUSHR = SPI_PUSHR_PCS(1U << st->current_slave) | SPI_PUSHR_TXDATA(0xFF);
			} else {
				break;
			}
		}

		// disable TFFF if nothing left to drive
		if (st->tx_buf.count == 0 && !st->rx_pending) {
			st->tx_pending = false;
			spi->RSER &= ~SPI_RSER_TFFF_RE_MASK;
		}
	}
}

void SPI0_IRQHandler(void) {
	_spi_irq_handler(0);
}
void SPI1_IRQHandler(void) {
	_spi_irq_handler(1);
}
void SPI2_IRQHandler(void) {
	_spi_irq_handler(2);
}

/******************************************HELPERS******************************************************/
static void _spi_drv_set_baudrate(uint8_t spi_num, uint32_t baud) {
	// prescaler values, CTAR PBR field (0 to 3
	static const uint8_t pbr_vals[] = {2, 3, 5, 7};
	// scaler values, TAR BR field (0-15)
	static const uint32_t br_vals[] = {2, 4, 6, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};

	uint32_t bus_clk = __CORE_CLOCK__ >> 1; // SPI uses bus clock = core/2

	// clamp baud rate to valid range
	baud = (baud == 0) ? SPI_HAL_DEFAULT_BAUDRATE : (baud > bus_clk / 2) ? SPI_HAL_DEFAULT_BAUDRATE : baud;

	// brute force over all 64 combinations!!
	uint32_t best_diff = UINT32_MAX;
	uint8_t best_pbr = 0;
	uint8_t best_br = 0;

	for (uint8_t i = 0; i < 4; i++) {
		for (uint8_t j = 0; j < 16; j++) {
			uint32_t actual = bus_clk / (pbr_vals[i] * br_vals[j]);
			uint32_t diff = (actual > baud) ? (actual - baud) : (baud - actual);
			if (diff < best_diff) {
				best_diff = diff;
				best_pbr = i;
				best_br = j;
			}
		}
	}

	// apply only baud fields, preserve the rest
	spi_ptrs[spi_num]->CTAR[0] = (spi_ptrs[spi_num]->CTAR[0] & ~(SPI_CTAR_PBR_MASK | SPI_CTAR_BR_MASK)) |
								 SPI_CTAR_PBR(best_pbr) | SPI_CTAR_BR(best_br);
}

static uint8_t _spi_get_next_device(uint8_t spi_num, uint8_t slave_num) {
	if (slave_num >= SPI_MAX_DEVICE_COUNT) {
		return 0;
	}
	return spi_pin_map[spi_num].pcsn[slave_num].pcs; // 0 means not available
}

static void _spi_drain_rx_fifo(uint8_t spi_num) {
	SPI_Type *spi = spi_ptrs[spi_num];
	SPIState_t *st = &spi_state[spi_num];

	// disable RFDF interrupt
	spi->RSER &= ~SPI_RSER_RFDF_RE_MASK;

	while (spi->SR & SPI_SR_RXCTR_MASK) {
		uint8_t byte = (uint8_t) (spi->POPR);
		_buf_push(&st->rx_buf, byte); // drop if SW buf full @todo change later
	}
	spi->SR = SPI_SR_RFDF_MASK;

	// re-enable RFDF interrupt
	spi->RSER |= SPI_RSER_RFDF_RE_MASK;
}

/****************************BUFFER HELPERS *****************************/

static bool _buf_push(SPIRingBuff_t *buf, uint8_t byte) {
	if (buf->count >= SPI_BUFF_SIZE) {
		return false; // full
	}
	buf->buf[buf->head] = byte;
	buf->head = (buf->head + 1) % SPI_BUFF_SIZE;
	buf->count++;
	return true;
}

static bool _buf_pop(SPIRingBuff_t *buf, uint8_t *byte) {
	if (buf->count == 0) {
		return false; // empty
	}
	*byte = buf->buf[buf->tail];
	buf->tail = (buf->tail + 1) % SPI_BUFF_SIZE;
	buf->count--;
	return true;
}
