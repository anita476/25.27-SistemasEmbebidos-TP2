#include "include/uart_test.h"
#include "../../drivers/MCAL/include/uart.h"
#include <string.h>

bool uart_test(uint8_t uart_id) {
	const char *start_msg = "\r\n=== UART TEST START ===\r\n"
							"Type something. Press Ctrl+A to exit.\r\n";

	UART_data_transmit(uart_id, (unsigned char *) start_msg, strlen(start_msg));

	uint8_t rx_buffer[BUFFER_SIZE];
	bool running = true;

	while (running) {
		if (UART_rstatus(uart_id)) {
			uint8_t n = UART_data_receive(uart_id, rx_buffer, BUFFER_SIZE);

			for (uint8_t i = 0; i < n; i++) {
				uint8_t c = rx_buffer[i];

				if (c == CTRL_A) {
					const char *exit_msg = "\n[CTRL + A RECEIVED - EXITING]\n";
					UART_data_transmit(uart_id, (unsigned char *) exit_msg, strlen(exit_msg));
					running = false;
					break;
				}
				if (c == '\r' || c == '\n') {
					const char *newline = "\r\n";
					while (UART_data_transmit(uart_id, (unsigned char *) newline, 2) == 0)
						;
				}

				// Echo character
				if (UART_tstatus(uart_id)) {
					UART_data_transmit(uart_id, &c, 1);
					printf("Wrote %c to transmit\n", (char) c);
				}
			}
		}
	}

	// Optional: final state
	const char *done_msg = "UART test finished.\r\n";
	UART_data_transmit(uart_id, (unsigned char *) done_msg, strlen(done_msg));

	return true;
}
