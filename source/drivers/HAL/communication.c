#include "include/communication.h"
#include "../MCAL/include/uart.h"
#include "include/board.h"

#include <stddef.h>

#define COMM_LED_CMD_MASK_FIXED_BITS 0x88u
#define COMM_LED_CMD_FIXED_BITS 0x80u

#define COMM_LED_GROUP_SHIFT 4u
#define COMM_LED_GROUP_MASK 0x07u

#define COMM_LED_RED_MASK 0x04u
#define COMM_LED_GREEN_MASK 0x02u
#define COMM_LED_BLUE_MASK 0x01u

#define COMM_MAX_ANGLE_VALUE_LEN 4u
#define COMM_MAX_ANGLE_FRAME_LEN (1u + COMM_MAX_ANGLE_VALUE_LEN)

static uint8_t comm_uart_id = INVALID_UART;

/******** forwards decs *********************/
static bool comm_is_initialized(void);
static bool comm_is_valid_angle_id(CommAngleId angle_id);
static bool comm_is_valid_angle_ascii(const char *value, uint8_t length);
static uint8_t comm_format_int(int16_t value, char *out);
/*******************************************/

bool communication_drv_init() {
	uint32_t uart_id = UART_drv_instance_init(PIN_UART0_RX, PIN_UART0_TX, UART0_BAUDRATE);

	if (uart_id == INVALID_UART) {
		comm_uart_id = INVALID_UART;
		return false;
	}

	comm_uart_id = (uint8_t) uart_id;
	return true;
}

/**
 * @brief Polls uart0 and decodes a led command. nonblocking but should be called from main loop
 *
 * LED command format:
 *   bit 7    = 1
 *   bits 6-4 = JKL group number
 *   bit 3    = 0
 *   bits 2-0 = RGB LED state
 *
 * @returns true when a valid LED command was decoded into out_cmd.
 */
bool communication_drv_receive_led_cmd(CommLedCmd_t *out_cmd) {
	uint8_t rx_buffer[8];
	if ((out_cmd == NULL) || !comm_is_initialized() || !UART_rstatus(comm_uart_id)) {
		return false;
	}
	uint8_t count = UART_data_receive(comm_uart_id, rx_buffer, sizeof(rx_buffer));
	for (uint8_t i = 0u; i < count; i++) {
		uint8_t byte = rx_buffer[i];
		if ((byte & COMM_LED_CMD_MASK_FIXED_BITS) == COMM_LED_CMD_FIXED_BITS) {
			out_cmd->group = (byte >> COMM_LED_GROUP_SHIFT) & COMM_LED_GROUP_MASK;
			out_cmd->red = (byte & COMM_LED_RED_MASK) != 0u;
			out_cmd->green = (byte & COMM_LED_GREEN_MASK) != 0u;
			out_cmd->blue = (byte & COMM_LED_BLUE_MASK) != 0u;
			return true;
		}
	}
	return false;
}

// @todo revisar...
bool communication_drv_send_angle_ascii(CommAngleId angle_id, const char *angle_value, uint8_t angle_value_len) {
	uint8_t frame[COMM_MAX_ANGLE_FRAME_LEN];

	if (!comm_is_initialized() || !comm_is_valid_angle_id(angle_id) ||
		!comm_is_valid_angle_ascii(angle_value, angle_value_len)) {
		return false;
	}

	frame[0] = (uint8_t) angle_id;

	for (uint8_t i = 0u; i < angle_value_len; i++) {
		frame[i + 1u] = (uint8_t) angle_value[i];
	}

	return communication_drv_send_raw(frame, (uint8_t) (angle_value_len + 1u));
}

bool communication_drv_send_angle_int(CommAngleId angle_id, int16_t angle_value) {
	char value[COMM_MAX_ANGLE_VALUE_LEN];
	uint8_t value_len = comm_format_int(angle_value, value);

	if (value_len == 0u) {
		return false;
	}

	return communication_drv_send_angle_ascii(angle_id, value, value_len);
}

bool communication_drv_send_raw(const uint8_t *data, uint8_t length) {
	if ((data == NULL) || (length == 0u) || !comm_is_initialized() || !UART_tstatus(comm_uart_id)) {
		return false;
	}

	return UART_data_transmit(comm_uart_id, (unsigned char *) data, length) == length;
}

/********************************************** HELPERS ********************++******/
static bool comm_is_initialized(void) {
	return comm_uart_id != INVALID_UART;
}

static bool comm_is_valid_angle_id(CommAngleId angle_id) {
	return (angle_id == COMM_ANGLE_ROLL) || (angle_id == COMM_ANGLE_PITCH) || (angle_id == COMM_ANGLE_ORIENTATION);
}

static bool comm_is_valid_angle_ascii(const char *value, uint8_t length) {
	uint8_t digit_start = 0u;

	if ((value == NULL) || (length == 0u) || (length > COMM_MAX_ANGLE_VALUE_LEN)) {
		return false;
	}

	if ((value[0] == '-') || (value[0] == '+')) {
		if (length == 1u) {
			return false;
		}

		digit_start = 1u;
	}

	for (uint8_t i = digit_start; i < length; i++) {
		if ((value[i] < '0') || (value[i] > '9')) {
			return false;
		}
	}

	return true;
}

static uint8_t comm_format_int(int16_t value, char *out) {
	uint8_t length = 0u;
	uint16_t magnitude;
	char reversed[COMM_MAX_ANGLE_VALUE_LEN];
	uint8_t reversed_len = 0u;

	if (value < 0) {
		out[length++] = '-';
		magnitude = (uint16_t) (-value);
	} else {
		magnitude = (uint16_t) value;
	}

	do {
		if ((length + reversed_len) >= COMM_MAX_ANGLE_VALUE_LEN) {
			return 0u;
		}

		reversed[reversed_len++] = (char) ('0' + (magnitude % 10u));
		magnitude /= 10u;
	} while (magnitude > 0u);

	while (reversed_len > 0u) {
		out[length++] = reversed[--reversed_len];
	}

	return length;
}
