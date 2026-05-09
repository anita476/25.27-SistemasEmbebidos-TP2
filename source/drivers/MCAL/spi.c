#include "include/spi.h"
#include "include/gpio.h"
#include "include/port.h"
#include <stdint.h>

#define SPI_HAL_DEFAULT_BAUDRATE 1000000UL // 1 MHz
#define SPI_COUNT 3
#define SPI_MAX_DEVICE_COUNT 6 // not all are implemented in all spi modules
#define SPI_HW_FIFO_DEPTH 4

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
	volatile bool tx_pending;		  // queued in tx_buf
	volatile uint8_t rx_expected;	  // bytes still to clock in and store
	volatile uint8_t total_remaining; // bytes left in  transaction  -> CONT

	volatile bool *done_flag; // set true by ISR when transaction is complete

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
	port_ptrs[spi_pin_map[spi_num].port]->PCR[PIN2NUM(spi_pin_map[spi_num].mosi)] =
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

	spi_state[spi_num] = ((SPIState_t) {
		.active = true,
		.slave_count = 0,
		.current_slave = 0,
		.tx_pending = false,
		.rx_expected = 0,
		.total_remaining = 0,
		.done_flag = NULL,
		.tx_buf = {0},
		.rx_buf = {0},
	});

	return true;
}

/*
 * @brief Initialize a device in spi bus. Slaves supported depends on module.
 * @returns The slave number, or -1 if an error ocurred.
 */
int8_t spi_drv_add_slave(uint8_t spi_num) {
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
 * @brief TX only: Queues to transfer buffer to send to slave. Non blocking. CS held low for entire len bytes via CONT.
 * @param spi_num Spi module
 * @param slave_num Selected slave
 * @returns Number of bytes efectively queued into buffer. 0 on error
 */
uint8_t spi_drv_write(uint8_t spi_num, uint8_t slave_num, const uint8_t *tx_data, size_t len,
					  volatile bool *done_flag) {
	if (spi_num >= SPI_COUNT || !spi_state[spi_num].active)
		return 0;
	if (slave_num >= spi_state[spi_num].slave_count || tx_data == NULL || len == 0)
		return 0;

	SPIState_t *st = &spi_state[spi_num];
	st->current_slave = slave_num;
	st->rx_expected = 0;
	st->done_flag = done_flag;
	if (done_flag != NULL)
		*done_flag = false;

	uint8_t queued = 0;
	for (size_t i = 0; i < len; i++) {
		if (!_buf_push(&st->tx_buf, tx_data[i]))
			break;
		queued++;
	}
	if (queued > 0) {
		st->total_remaining = queued;
		st->tx_pending = true;
		spi_ptrs[spi_num]->RSER |= SPI_RSER_TFFF_RE_MASK;
	}
	return queued;
}

/*
 * @brief Send instruction bytes then clock in response bytes, CS held throughout.
		  Non-blocking since ISR sets *done_flag when all rx_len bytes are stored
 *        Retrieve data afterwards with spi_drv_read()!!
 * @returns bytes queued into TX buffer, 0 on error.
 */
uint8_t spi_drv_transact(uint8_t spi_num, uint8_t slave_num, const uint8_t *tx_data, size_t tx_len, size_t rx_len,
						 volatile bool *done_flag) {
	if (spi_num >= SPI_COUNT || !spi_state[spi_num].active)
		return 0;
	if (slave_num >= spi_state[spi_num].slave_count)
		return 0;
	if (tx_data == NULL || tx_len == 0)
		return 0;

	SPIState_t *st = &spi_state[spi_num];
	st->current_slave = slave_num;
	st->rx_expected = (uint8_t) rx_len;
	st->total_remaining = (uint8_t) (tx_len + rx_len);
	st->done_flag = done_flag;
	if (done_flag != NULL)
		*done_flag = false;

	uint8_t queued = 0;
	for (size_t i = 0; i < tx_len; i++) {
		if (!_buf_push(&st->tx_buf, tx_data[i]))
			break;
		queued++;
	}
	if (queued > 0) {
		st->tx_pending = true;
		spi_ptrs[spi_num]->RSER |= SPI_RSER_TFFF_RE_MASK;
	}
	return queued;
}

/*
 * @brief Copy len bytes from RX SW buffer into rx_buf.
 *        Call after done_flag is set. Falls back to draining HW FIFO if needed.
 * @returns true if len bytes were available and copied.
 */
bool spi_drv_read(uint8_t spi_num, uint8_t slave_num, uint8_t *rx_buf, size_t len) {
	if (spi_num >= SPI_COUNT || !spi_state[spi_num].active)
		return false;
	if (slave_num >= spi_state[spi_num].slave_count || rx_buf == NULL || len == 0)
		return false;

	SPIState_t *st = &spi_state[spi_num];

	if (st->rx_buf.count < len) {
		_spi_drain_rx_fifo(spi_num); // rescue bytes still in HW FIFO
	}
	if (st->rx_buf.count < len)
		return false;

	for (size_t i = 0; i < len; i++) {
		_buf_pop(&st->rx_buf, &rx_buf[i]);
	}
	return true;
}

/**
 * @brief Number of bytes in use in tx buffer
 *
 */
uint8_t spi_drv_tx_busy(uint8_t spi_num) {
	if (spi_num >= SPI_COUNT || !spi_state[spi_num].active)
		return 0;
	uint8_t hw = (uint8_t) ((spi_ptrs[spi_num]->SR & SPI_SR_TXCTR_MASK) >> SPI_SR_TXCTR_SHIFT);
	return spi_state[spi_num].tx_buf.count + hw;
}

/* @brief Bytes available to read (SW RX buffer + HW RX FIFO). */
uint8_t spi_drv_rx_available(uint8_t spi_num) {
	if (spi_num >= SPI_COUNT || !spi_state[spi_num].active)
		return 0;
	uint8_t hw = (uint8_t) ((spi_ptrs[spi_num]->SR & SPI_SR_RXCTR_MASK) >> SPI_SR_RXCTR_SHIFT);
	return spi_state[spi_num].rx_buf.count + hw;
}

/*****************************************INTERRUPTS  ROUTINES******************************************/

static void _spi_irq_handler(uint8_t spi_num) {
	SPI_Type *spi = spi_ptrs[spi_num];
	uint32_t sr = spi->SR;
	SPIState_t *st = &spi_state[spi_num];

	//  RX
	if (sr & SPI_SR_RFDF_MASK) {
		spi->SR = SPI_SR_RFDF_MASK; // w1c — clear before draining
		uint8_t count = (uint8_t) ((spi->SR & SPI_SR_RXCTR_MASK) >> SPI_SR_RXCTR_SHIFT);
		while (count--) {
			uint8_t byte = (uint8_t) (spi->POPR);
			if (st->rx_expected > 0) {
				_buf_push(&st->rx_buf, byte);
				st->rx_expected--;

				// notify done
				if (st->rx_expected == 0 && st->done_flag != NULL) {
					*st->done_flag = true;
				}
			}
			// else is echo from TX only ,discard
		}
	}

	//  TX: fill HW FIFO, CONT=1 on all but last byte of transaction
	if (sr & SPI_SR_TFFF_MASK) {
		uint8_t byte;
		uint8_t pushed = 0;

		while (((spi->SR & SPI_SR_TXCTR_MASK) >> SPI_SR_TXCTR_SHIFT) < SPI_HW_FIFO_DEPTH) {
			bool has_real = st->tx_pending && _buf_pop(&st->tx_buf, &byte);
			bool need_dummy = !has_real && (st->rx_expected > 0);

			if (!has_real && !need_dummy)
				break;

			if (!has_real)
				byte = 0xFF; // dummy byte to clock in RX response

			// st->total_remaining--;
			//  drop CONT on the very last byte to release CS
			// uint32_t cont = (st->total_remaining > 0) ? SPI_PUSHR_CONT_MASK : 0U;
			bool more_frames = (st->tx_buf.count > 0) || (st->rx_expected > 1);

			uint32_t cont = more_frames ? SPI_PUSHR_CONT_MASK : 0U;

			if (st->total_remaining > 0) {
				st->total_remaining--;
			}

			spi->PUSHR = cont | SPI_PUSHR_PCS(1U << st->current_slave) | SPI_PUSHR_TXDATA(byte);
			pushed++;
		}

		// clear TFFF only after writing to FIFO
		if (pushed > 0) {
			spi->SR = SPI_SR_TFFF_MASK;
		}

		if (st->tx_buf.count == 0) {
			st->tx_pending = false;
		}

		// nothing left to drive: disable TFFF
		// for write-only also set done_flag here
		if (!st->tx_pending && st->rx_expected == 0) {
			if (st->done_flag != NULL && !(*st->done_flag)) {
				*st->done_flag = true; // write-only completion
			}
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
