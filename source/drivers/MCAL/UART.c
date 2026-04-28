#include "include/uart.h"
#include "include/port.h"
#define UART_ALTS 5
#define MAX_UART_USE 5 /* Max number of uarts that can be in use at any given time*/
#define BUFFER_CHAR_SIZE 32
#define UART_HAL_DEFAULT_BAUDRATE 9600
#define UART_SENTINEL 0xFF
typedef struct {
	uint32_t uart_num;
	uint32_t uart_port;
	pin_t tx_pin;
	pin_t rx_pin;
	PORTMux_t alt;
} UARTConfigNBF_t;

// for now only one pin set per uart number
static UARTConfigNBF_t uart_pin_map_NBF[UART_COUNT] = {{0, PB, PORTNUM2PIN(PB, 17), PORTNUM2PIN(PB, 16), PORT_mAlt3},
													   {1, PE, PORTNUM2PIN(PE, 0), PORTNUM2PIN(PE, 1), PORT_mAlt3},
													   {2, PD, PORTNUM2PIN(PD, 3), PORTNUM2PIN(PD, 2), PORT_mAlt3},
													   {3, PC, PORTNUM2PIN(PC, 17), PORTNUM2PIN(PC, 16), PORT_mAlt3},
													   {4, PE, PORTNUM2PIN(PE, 24), PORTNUM2PIN(PE, 25), PORT_mAlt3}};

static UART_Type *const uart_ptrs_NBF[] = UART_BASE_PTRS;

// Lookup for clock gating and nvic ints
uint32_t const uart_sim_masks_NBF[UART_COUNT] = {SIM_SCGC4_UART0_MASK, SIM_SCGC4_UART1_MASK, SIM_SCGC4_UART2_MASK,
												 SIM_SCGC4_UART3_MASK, SIM_SCGC1_UART4_MASK};
uint32_t const uart_nvim_ints_NBF[UART_COUNT] = {UART0_RX_TX_IRQn, UART1_RX_TX_IRQn, UART2_RX_TX_IRQn, UART3_RX_TX_IRQn,
												 UART4_RX_TX_IRQn};

/********** Store buffers for all uarts ************/
typedef struct {
	uint8_t id;
	/***************** CRITICAL ACCESS ZONE ************/
	uint8_t buffer[BUFFER_CHAR_SIZE];
	uint8_t _head; /* WRITE HERE*/
	uint8_t _tail; /* READ FROM HERE */
				   /***************************************************/
} UartBuffer_t;

typedef struct {
	UartBuffer_t tx;
	UartBuffer_t rx;
	bool active;	 // true once init'd
	uint8_t tx_size; // cached at init
	uint8_t rx_size; // cached at init
} UartState_t;

/* uart states ! */
static volatile UartState_t uart_state_NBF[MAX_UART_USE];
volatile uint8_t uart_count_NBF;

/* forward declarations */
static uint8_t buf_next(uint8_t idx);
static bool buf_is_empty(const UartBuffer_t *b);
static bool buf_is_full(const UartBuffer_t *b);
static bool buf_enqueue(UartBuffer_t *b, uint8_t byte);
static uint8_t buf_dequeue(UartBuffer_t *b);
static uint8_t buf_count(const UartBuffer_t *b);
static uint32_t find_uart_id(pin_t tx, pin_t rx);
static uint8_t decode_size(uint8_t encoded);

uint32_t UART_drv_instance_init(pin_t RX_pin, pin_t TX_pin) {
	uint8_t id = find_uart_id(TX_pin, RX_pin);
	if (id == INVALID_UART || uart_count_NBF >= MAX_UART_USE) {
		return INVALID_UART;
	}

	// enable clock for the pin port
	SIM->SCGC5 |= port_clock_masks[PIN2PORT(uart_pin_map_NBF[id].rx_pin)];

	// enable clock for uart module
	if (id != 4) {
		SIM->SCGC4 |= uart_sim_masks_NBF[id];
	} else {
		SIM->SCGC1 |= uart_sim_masks_NBF[id];
	}

	// enable nvic for that uart
	NVIC_EnableIRQ(uart_nvim_ints_NBF[id]);

	UART_set_baudrate(id, 9600);

	/**CONFIGURING FIFO  **/
	// 4 BYTES WATERMAKS
	uart_ptrs_NBF[id]->RWFIFO = 4; // RX
	uart_ptrs_NBF[id]->TWFIFO = 2; // TX

	// Enable TX and RX FIFOs, flush both
	uart_ptrs_NBF[id]->PFIFO |= UART_PFIFO_TXFE_MASK | UART_PFIFO_RXFE_MASK;
	uart_ptrs_NBF[id]->CFIFO |= UART_CFIFO_TXFLUSH_MASK | UART_CFIFO_RXFLUSH_MASK;

	// decode fifo sizes
	uart_state_NBF[id].tx_size =
		decode_size((uart_ptrs_NBF[id]->PFIFO & UART_PFIFO_TXFIFOSIZE_MASK) >> UART_PFIFO_TXFIFOSIZE_SHIFT);
	uart_state_NBF[id].rx_size =
		decode_size((uart_ptrs_NBF[id]->PFIFO & UART_PFIFO_RXFIFOSIZE_MASK) >> UART_PFIFO_RXFIFOSIZE_SHIFT);

	/* configure rx and tx pins */
	port_ptrs[uart_pin_map_NBF[id].uart_port]->PCR[PIN2NUM(uart_pin_map_NBF[id].tx_pin)] = 0x0;
	port_ptrs[uart_pin_map_NBF[id].uart_port]->PCR[PIN2NUM(uart_pin_map_NBF[id].tx_pin)] |=
		PORT_PCR_MUX(uart_pin_map_NBF[id].alt);
	port_ptrs[uart_pin_map_NBF[id].uart_port]->PCR[PIN2NUM(uart_pin_map_NBF[id].tx_pin)] |=
		PORT_PCR_IRQC(PORT_eDisabled);

	port_ptrs[uart_pin_map_NBF[id].uart_port]->PCR[PIN2NUM(uart_pin_map_NBF[id].rx_pin)] = 0x0;
	port_ptrs[uart_pin_map_NBF[id].uart_port]->PCR[PIN2NUM(uart_pin_map_NBF[id].rx_pin)] |=
		PORT_PCR_MUX(uart_pin_map_NBF[id].alt);
	port_ptrs[uart_pin_map_NBF[id].uart_port]->PCR[PIN2NUM(uart_pin_map_NBF[id].rx_pin)] |=
		PORT_PCR_IRQC(PORT_eDisabled);

	// enable uart transmission &&& rie interrupt
	/*****
	 * Remember that: RIE is Receive Interrupt Enable
	 * So an interrupt is triggered when RDRF goes HIGH (8bit buff is full)
	 */
	uart_ptrs_NBF[id]->C2 = UART_C2_TE_MASK | UART_C2_RE_MASK | UART_C2_RIE_MASK;

	uart_state_NBF[id].tx.id = id;
	uart_state_NBF[id].tx._head = 0;
	uart_state_NBF[id].tx._tail = 0;
	uart_state_NBF[id].rx.id = id;
	uart_state_NBF[id].rx._head = 0;
	uart_state_NBF[id].rx._tail = 0;
	uart_state_NBF[id].active = true;
	uart_count_NBF++;
	return id;
}

void UART_set_baudrate(uint8_t uart_id, uint32_t baudrate) {
	uint16_t sbr, brfa;
	uint32_t clock;

	clock = ((uart_id == 0) || (uart_id == 1)) ? (__CORE_CLOCK__) : (__CORE_CLOCK__ >> 1);
	baudrate = ((baudrate == 0) ? (UART_HAL_DEFAULT_BAUDRATE) :
								  ((baudrate > 0x1FFF) ? (UART_HAL_DEFAULT_BAUDRATE) : (baudrate)));

	sbr = clock / (baudrate << 4);				 // sbr = clock/(Baudrate x 16)
	brfa = (clock << 1) / baudrate - (sbr << 5); // brfa = 2*Clock/baudrate - 32*sbr

	uart_ptrs_NBF[uart_id]->BDH = UART_BDH_SBR(sbr >> 8);
	uart_ptrs_NBF[uart_id]->BDL = UART_BDL_SBR(sbr);
	uart_ptrs_NBF[uart_id]->C4 = (uart_ptrs_NBF[uart_id]->C4 & ~UART_C4_BRFA_MASK) | UART_C4_BRFA(brfa);
}

uint8_t UART_data_transmit(uint8_t uart_id, unsigned char *txdata, uint8_t length) {
	if (!uart_state_NBF[uart_id].active)
		return 0;

	// producer - consumer -> protect buffer HEAD
	// NVIC_DisableIRQ(uart_nvim_ints_NBF[uart_id]);
	uint8_t sent = 0;
	for (uint8_t i = 0; i < length; i++) {
		if (!buf_enqueue((UartBuffer_t *) &uart_state_NBF[uart_id].tx, txdata[i]))
			break;
		sent++;
	}
	if (sent > 0)
		uart_ptrs_NBF[uart_id]->C2 |= UART_C2_TIE_MASK;
	// NVIC_EnableIRQ(uart_nvim_ints_NBF[uart_id]);
	return sent; // caller knows if bytes were dropped
}

uint8_t UART_data_receive(uint8_t uart_id, uint8_t *out, uint8_t length) {
	if (!uart_state_NBF[uart_id].active)
		return 0;

	NVIC_DisableIRQ(uart_nvim_ints_NBF[uart_id]);

	//
	uint8_t sw_available = buf_count((UartBuffer_t *) &uart_state_NBF[uart_id].rx);
	if (sw_available < length) { // only drain fifo if available in sf buff is less than requested!
		uint8_t hw_available = uart_ptrs_NBF[uart_id]->RCFIFO;
		while (hw_available--) {
			buf_enqueue((UartBuffer_t *) &uart_state_NBF[uart_id].rx, uart_ptrs_NBF[uart_id]->D);
		}
	}
	NVIC_EnableIRQ(uart_nvim_ints_NBF[uart_id]);

	uint8_t read = 0;
	while (read < length && !buf_is_empty((UartBuffer_t *) &uart_state_NBF[uart_id].rx)) {
		out[read++] = buf_dequeue((UartBuffer_t *) &uart_state_NBF[uart_id].rx);
	}

	return read;
}

static uint32_t find_uart_id(pin_t tx, pin_t rx) {
	int i = 0;
	while (i != UART_ALTS) {
		if (uart_pin_map_NBF[i].tx_pin == tx && uart_pin_map_NBF[i].rx_pin == rx) {
			return uart_pin_map_NBF[i].uart_num;
		}
		i++;
	}
	return INVALID_UART;
}

bool UART_tstatus(uint8_t uart_id) {
	if (!uart_state_NBF[uart_id].active)
		return false;
	return !buf_is_full((UartBuffer_t *) &uart_state_NBF[uart_id].tx);
}

bool UART_rstatus(uint8_t uart_id) {
	if (!uart_state_NBF[uart_id].active)
		return false;

	return !buf_is_empty((UartBuffer_t *) &uart_state_NBF[uart_id].rx) ||
		   (uart_ptrs_NBF[uart_id]->RCFIFO > 0); // checks buffer AND fifo!
}

void UART_RX_TX_ISR(uint8_t id) {
	UART_Type *uart = uart_ptrs_NBF[id];
	uint8_t s1 = uart->S1;

	// Drain the RX hardware FIFO into sf buff
	if (s1 & UART_S1_RDRF_MASK) {
		// RDRF fires at watermark, we keep reading until nothing is left to rcv
		uint8_t count = uart->RCFIFO; // bytes currently in hw RX FIFO
		while (count--) {
			uint8_t byte = uart->D;
			buf_enqueue((UartBuffer_t *) &uart_state_NBF[id].rx, byte);
		}
	}

	// fill  TX hw FIFO from sft buff
	if ((s1 & UART_S1_TDRE_MASK) && (uart->C2 & UART_C2_TIE_MASK)) {
		uint8_t space = uart_state_NBF[id].tx_size - uart->TCFIFO;
		while (space-- && !buf_is_empty((UartBuffer_t *) &uart_state_NBF[id].tx)) {
			uart->D = buf_dequeue((UartBuffer_t *) &uart_state_NBF[id].tx);
		}
		// disable tie when buff is empty
		if (buf_is_empty((UartBuffer_t *) &uart_state_NBF[id].tx)) {
			uart->C2 &= ~UART_C2_TIE_MASK;
		}
	}
}

/************* UART INTERRUPT HANDLERS ********************/
__ISR__ UART0_RX_TX_IRQHandler(void) {
	UART_RX_TX_ISR(0);
}
__ISR__ UART1_RX_TX_IRQHandler(void) {
	UART_RX_TX_ISR(1);
}
__ISR__ UART2_RX_TX_IRQHandler(void) {
	UART_RX_TX_ISR(2);
}
__ISR__ UART3_RX_TX_IRQHandler(void) {
	UART_RX_TX_ISR(3);
}
__ISR__ UART4_RX_TX_IRQHandler(void) {
	UART_RX_TX_ISR(4);
}

/**s tatics */
static uint8_t buf_next(uint8_t idx) {
	return (idx + 1) % BUFFER_CHAR_SIZE;
}

static bool buf_is_empty(const UartBuffer_t *b) {
	return b->_head == b->_tail;
}

static bool buf_is_full(const UartBuffer_t *b) {
	return buf_next(b->_head) == b->_tail;
}
static uint8_t buf_count(const UartBuffer_t *b) {
	return (b->_head - b->_tail + BUFFER_CHAR_SIZE) % BUFFER_CHAR_SIZE;
}
/** Returns false if buffer was full and byte was dropped) */
static bool buf_enqueue(UartBuffer_t *b, uint8_t byte) {
	if (buf_is_full(b))
		return false;
	b->buffer[b->_head] = byte;
	b->_head = buf_next(b->_head);
	return true;
}

static uint8_t buf_dequeue(UartBuffer_t *b) {
	if (buf_is_empty(b))
		return UART_SENTINEL; /* should never get to this */
	uint8_t byte = b->buffer[b->_tail];
	b->_tail = buf_next(b->_tail);
	return byte;
}

static uint8_t decode_size(uint8_t encoded) {
	if (encoded == 0)
		return 1;
	return 1 << (encoded + 1);
}
