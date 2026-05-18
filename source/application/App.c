/***************************************************************************/ /**
   @file     App.c
   @brief    Application functions
   @author   Nicolás Magliola
  ******************************************************************************/
#include "../drivers/HAL/include/acc_magn.h"
#include "../drivers/HAL/include/board_led.h"
#include "../drivers/HAL/include/bus_comm.h"
#include "../drivers/HAL/include/terminal_comm.h"
#include "../drivers/HAL/include/timer.h"
#include "tests/include/spi_test.h"
#include "tests/include/uart_test.h"
#include <stdlib.h>

#define ANGLE_TYPE_COUNT 2U
/* obs! we put the timers at half time because it has a bug : one angle type is sent every time, alternating */
#define ANGLE_CHANGE_PERIOD_MS 25U		/* max frequency      */
#define ANGLE_KEEPALIVE_PERIOD_MS 1000U /* minimum frequency      @todo because they are being sent double now..    */
#define ANGLE_CHANGE_THRESHOLD 5

typedef struct {
	char id;
	angle_t last_sent; /* reference for threshold -> updated after sending can frames*/
	bool pending;
	angle_t pending_value;
	bool uart_done;
} angle_state_t;

static angle_state_t g_angles[ANGLE_TYPE_COUNT]; /* 0=pitch, 1=roll   */
static uint32_t g_rate_timer;
static uint32_t g_keepalive_timer;

static uint8_t g_tx_channel = 0U;
static bool g_tx_busy = false;
static uint32_t g_tx_watchdog_timer;

static void angles_init(void);
static int16_t angle_delta(angle_t current, angle_t reference);
static void angles_check_rate(void);
static void angles_check_keepalive(void);
static void send_pending(void);
static void on_can_tx_done(bool success);

void App_Init(void) {
	timer_drv_init();
	board_led_drv_init();
	if (!terminal_comm_drv_init()) {
		board_led_drv_state(RED, true);
	}
}

void App_Run(void) {
	// controller and acc need interrupts enabled to init
	if (!can_controller_drv_init()) {
		board_led_drv_state(RED, true);
		while (1) {
			;
		}
	} else {
		board_led_drv_state(GREEN, true);
	}
	acc_magn_drv_init();
	angles_init();

	// application
	while (1) {
		timer_drv_update();
		can_process();

		/* Incoming CAN frames */
		CanFrame_t rx_frame;
		while (can_available()) {
			if (can_read(&rx_frame)) {
				process_can_frame(rx_frame);
			}
		}

		CommLedCmd_t led_cmd;
		if (terminal_comm_drv_receive_led_cmd(&led_cmd)) {
			if (led_cmd.group == (uint8_t) (CAN_GROUP & 0x07U)) {
				board_led_drv_state(RED, led_cmd.red);
				board_led_drv_state(GREEN, led_cmd.green);
				board_led_drv_state(BLUE, led_cmd.blue);
			} else {
				uint8_t led_byte = (0x80u) | ((led_cmd.group & 0x07u) << 4u) | (led_cmd.red ? 0x04u : 0u) |
								   (led_cmd.green ? 0x02u : 0u) | (led_cmd.blue ? 0x01u : 0u);
				can_send(&led_byte, 1u, NULL);
			}
		}
		if (timer_drv_expired(g_rate_timer)) {
			timer_drv_start(g_rate_timer, ANGLE_CHANGE_PERIOD_MS, TIM_MODE_SINGLESHOT, NULL);
			angles_check_rate();
		}
		if (timer_drv_expired(g_keepalive_timer)) {
			timer_drv_start(g_keepalive_timer, ANGLE_KEEPALIVE_PERIOD_MS, TIM_MODE_SINGLESHOT, NULL);
			angles_check_keepalive();
		}
		send_pending();
	}
}

/************************HELPERS*******************************************************************/

// INIT TIMERS and data
static void angles_init(void) {
	const char ids[3] = {'C', 'R', 'O'};

	for (uint8_t i = 0U; i < ANGLE_TYPE_COUNT; i++) {
		g_angles[i].id = ids[i];
		g_angles[i].last_sent = 0;
		g_angles[i].pending = false;
		g_angles[i].pending_value = 0;
		g_angles[i].uart_done = false;
	}
	g_rate_timer = timer_drv_get_id();
	g_keepalive_timer = timer_drv_get_id();
	g_tx_watchdog_timer = timer_drv_get_id();
	timer_drv_start(g_rate_timer, ANGLE_CHANGE_PERIOD_MS, TIM_MODE_SINGLESHOT, NULL);
	timer_drv_start(g_keepalive_timer, ANGLE_KEEPALIVE_PERIOD_MS, TIM_MODE_SINGLESHOT, NULL);
}

// Only mark pending if |delta| >= threshold
static void angles_check_rate(void) {
	sensor_t *s = acc_magn_drv_get_angles();
	const angle_t current[ANGLE_TYPE_COUNT] = {s->pitch, s->roll};

	for (uint8_t i = 0U; i < ANGLE_TYPE_COUNT; i++) {
		angle_state_t *a = &g_angles[i];
		int16_t delta = angle_delta(current[i], a->last_sent);
		if (abs(delta) >= ANGLE_CHANGE_THRESHOLD) {
			a->pending_value = current[i];
			a->pending = true;
			a->uart_done = false;
			a->last_sent = current[i];
		}
	}
}

static void angles_check_keepalive(void) {
	sensor_t *s = acc_magn_drv_get_angles();
	const angle_t current[ANGLE_TYPE_COUNT] = {s->pitch, s->roll};

	for (uint8_t i = 0U; i < ANGLE_TYPE_COUNT; i++) {
		g_angles[i].pending_value = current[i];
		g_angles[i].pending = true;
		g_angles[i].uart_done = false;
	}
}

static void send_pending(void) {
	/* first we enqueue uart -> handoff to periphral*/
	for (uint8_t i = 0U; i < ANGLE_TYPE_COUNT; i++) {
		angle_state_t *a = &g_angles[i];
		if (!a->pending || a->uart_done)
			continue;

		terminal_comm_drv_send_angle(a->id, a->pending_value);
		a->uart_done = true;
	}

	/* CAN pass -> needs to be one frame at a time since its a blocking func one frame at a time */
	if (g_tx_busy) {
		if (timer_drv_expired(g_tx_watchdog_timer)) {
			/* TX completion interrupt never fired -> RTS was likely lost? to
			 * next loop iteration will retry */
			bus_recover();
			g_tx_busy = false;
		} else {
			return;
		}
	}

	for (uint8_t i = 0U; i < ANGLE_TYPE_COUNT; i++) {
		uint8_t idx = (g_tx_channel + i) % ANGLE_TYPE_COUNT;
		angle_state_t *a = &g_angles[idx];

		if (!a->pending)
			continue;

		g_tx_busy = true;
		g_tx_channel = (uint8_t) ((idx + 1U) % ANGLE_TYPE_COUNT);
		timer_drv_start(g_tx_watchdog_timer, 50U, TIM_MODE_SINGLESHOT, NULL);

		if (!can_send_angle(a->pending_value, a->id, on_can_tx_done)) {
			g_tx_busy = false;
			return; /* bus busy, retry next loop */
		}

		a->pending = false;
		a->uart_done = false;
		return;
	}
}

static void on_can_tx_done(bool success) {
	g_tx_busy = false;
	if (!success) {
		/* restore the pending flag for the last sent angle
		 * so it can be retried on the next send_pending()  */
		uint8_t failed_idx = (g_tx_channel - 1 + ANGLE_TYPE_COUNT) % ANGLE_TYPE_COUNT;
		g_angles[failed_idx].pending = true;
	} else {
		send_pending();
	}
}

/*
 * accounts for wrap-around at +-180°.
 * returns the shortest angular distance between two angle values
 */
static int16_t angle_delta(angle_t current, angle_t reference) {
	int16_t d = (int16_t) (current - reference);
	/* wrap into (-180, 180] */
	if (d > 180)
		d -= 360;
	if (d < -180)
		d += 360;
	return d;
}