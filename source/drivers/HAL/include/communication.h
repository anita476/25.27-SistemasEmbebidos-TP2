#ifndef _COMMUNICATION_H_
#define _COMMUNICATION_H_

#include <stdbool.h>
#include <stdint.h>

/************************************** PC COMMUNICATION !!************************************+***********/

#define CURRENT_GROUP_ID 1U
typedef enum { COMM_ANGLE_ROLL = 'R', COMM_ANGLE_PITCH = 'C', COMM_ANGLE_ORIENTATION = 'O' } CommAngleId;

typedef struct {
	uint8_t group;
	bool red;
	bool green;
	bool blue;
} CommLedCmd_t;

/**
 * @brief Initialize communication driver (internally uses uart0)
 * @returns true on success, false if not
 */
bool communication_drv_init();

/**
 * @brief Polls uart0 and decodes a led command. nonblocking but should be called from main loop
 * @returns true when a valid LED command was decoded into out_cmd, false if not
 */
bool communication_drv_receive_led_cmd(CommLedCmd_t *out_cmd);

/**
 * @brief Send an angle frame using an already formatted ASCII value.
 *
 * Frame format:
 *   angleId angleVal
 *
 * angleId is 'R', 'C' or 'O'.
 * angleVal must contain 1 to 4 ASCII bytes. The string terminator is not sent.
 *
 * Valid examples: "R-34", "C0", "O67", "R+138", "R00", "C-072".
 */
bool communication_drv_send_angle_ascii(CommAngleId angle_id, const char *angle_value, uint8_t angle_value_len);

/**
 * @brief Send an angle frame from an integer value.
 *
 * The integer is converted to decimal ASCII without forced leading zeroes.
 * The resulting value, including sign when negative, must fit in 4 bytes ...
 */
bool communication_drv_send_angle_int(CommAngleId angle_id, int16_t angle_value);

/**
 * @brief Send raw bytes through the communication UART.
 */
bool communication_drv_send_raw(const uint8_t *data, uint8_t length);

#endif /* _COMMUNICATION_H_ */
