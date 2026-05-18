#include "include/bus_comm.h"
#include "include/board_led.h"
#include "include/terminal_comm.h"
#include <stdint.h>
#include <string.h>

#define UART_BUF_SIZE 16u
static void process_led_cmd(uint8_t led_bits);

static void process_led_cmd(uint8_t led_byte) {
	// last 3 bits are the actual are RGB, use
	// board_led_drv_state(RED, led_bits >> shift etc)
	board_led_drv_state(RED, (led_byte >> 2u) & 0x01u);
	board_led_drv_state(GREEN, (led_byte >> 1u) & 0x01u);
	board_led_drv_state(BLUE, (led_byte >> 0u) & 0x01u);
}

bool process_can_frame(CanFrame_t frame) {
	// if data < 2 then its a led command
	// if first 3 bits match CAN_GROUP,and execute
	// if they dont match, do nothing... maybe send the command via uart or smth

	// if data > 2 then its an angle
	// parse angle, convert to ascii and then queue send via uart
	// [ FIRST IS id (3 hexa), then add the rest of data bytes send them in hexa )

	if (frame.dlc == 0u) {
		return false;
	}
	/* if dlc = 1, led command*/
	if (frame.dlc == 1u) {
		uint8_t led_byte = frame.data[0];

		/* check if msb 3 bits of the LED byte match group id
		 * Format 1JKL 0RGB
		 * group is bits [6:4]
		 * */
		uint8_t frame_group = (led_byte >> 4u) & 0x07u;
		uint8_t our_group = (uint8_t) (CAN_GROUP & 0x07u);

		if (frame_group == our_group) {
			process_led_cmd(led_byte);
		} else { // @todo maybe discard later...
			/* 'L' + data byte */
			uint8_t buf[4];
			uint8_t pos = 0u;
			buf[pos++] = 'L';
			buf[pos++] = led_byte;
			buf[pos++] = '\r';
			buf[pos++] = '\n';
			terminal_comm_drv_send_raw((unsigned char *) buf, pos);
		}
		return true;
	}

	uint8_t angle_id = frame.data[0];
	if (angle_id != 'R' && angle_id != 'C' && angle_id != 'O') {
		return false;
	}
	if (frame.dlc > 5u) {
		return false; /* angleId (1 byte) + angle value*/
	}

	static const char hex[] = "0123456789ABCDEF";
	uint8_t buf[UART_BUF_SIZE];
	uint8_t pos = 0u;

	uint8_t dlc_copy = frame.dlc;
	uint32_t id_copy = frame.id;
	uint8_t data_copy[8];
	memcpy(data_copy, frame.data, dlc_copy);

	/* CAN ID as 3 hex digits */
	buf[pos++] = hex[(id_copy >> 8u) & 0x0Fu];
	buf[pos++] = hex[(id_copy >> 4u) & 0x0Fu];
	buf[pos++] = hex[id_copy & 0x0Fu];

	/* append  data bytes raw since theyre already ascii */
	for (uint8_t i = 0u; i < dlc_copy && i < 8u; i++) {
		buf[pos++] = data_copy[i];
	}

	buf[pos++] = '\r';
	buf[pos++] = '\n';

	terminal_comm_drv_send_raw((unsigned char *) buf, pos);

	return true;
}

bool can_send_angle(angle_t value, char angle_id, can_tx_cb_t cb) {
	static uint8_t buf[6]; /* 'R'/'C'/'O' + sign + up to 4 digits = 6 max*/
	uint8_t len = 0;

	/* first byte: angle identifier */
	buf[len++] = (uint8_t) angle_id;

	/* sign */
	if (value < 0) {
		buf[len++] = '-';
		value = (angle_t) (-value);
	} else {
		buf[len++] = '+';
	}

	/* absolute value as ASCII digits, no leading zeros */
	char digits[5];
	uint8_t ndigits = 0;
	uint16_t uval = (uint16_t) value;

	if (uval == 0) {
		digits[ndigits++] = '0';
	} else {
		while (uval > 0) {
			digits[ndigits++] = (char) ('0' + (uval % 10));
			uval /= 10;
		}
		/* reverse */
		for (uint8_t i = 0; i < ndigits / 2; i++) {
			char tmp = digits[i];
			digits[i] = digits[ndigits - 1 - i];
			digits[ndigits - 1 - i] = tmp;
		}
	}
	for (uint8_t i = 0; i < ndigits; i++) {
		buf[len++] = (uint8_t) digits[i];
	}
	return can_send(buf, len, cb);
}

void bus_recover(void) {
	can_recover();
}