/* ── switch.c ─────────────────────────────────────────────────────── */
#include "include/switch.h"
#include "../MCAL/include/pisr.h"
#include "include/timer.h"

#define DEBOUNCE_MS 20u
#define DOUBLE_CLICK_MS 300u
#define LONG_CLICK_MS 800u

#define ISACTIVE(curr, act) (!(curr ^ act))

static swEvent queue[SW_MAX_PENDING_EVENTS];
static volatile uint8_t _head = 0;
static volatile uint8_t _tail = 0;
static bool initialized = false;
typedef enum {
	SW_STATE_IDLE,
	SW_STATE_DEBOUNCE,
	SW_STATE_PRESSED,
	SW_STATE_RELEASED,
	SW_STATE_PRESSED_2, /* second press, measuring duration    */
	SW_STATE_WAIT_RELEASE,
} swState_t;

typedef struct {
	swState_t state;
	timId_t timer;
} swContext_t;

typedef struct {
	uint8_t pin;
	ACTIVE_ON active_on;
	bool registered;
} swConfig_t;

static swConfig_t config[SW_MAX_SWS];
static swContext_t ctx[SW_MAX_SWS];
static uint8_t sw_count = 0;

static void _switch_drv_push_event(swEvent ev);
static void _switch_drv_process_event(uint8_t i);

pisrCallbackPtr_t switch_drv_PISR(void);

void switch_drv_init() {
	if (initialized) {
		return;
	}
	pisr_drv_register((pisrCallbackPtr_t) switch_drv_PISR, 1);
	initialized = true;
}
sw_handle_t switch_drv_register(uint8_t pin, ACTIVE_ON active_level, PULL pullconfig) {
	if (sw_count >= SW_MAX_SWS)
		return INVALID_SW_HANDLE;

	timId_t tim = timer_drv_get_id();
	if (tim == TIMER_INVALID_ID)
		return INVALID_SW_HANDLE;

	uint8_t handle = sw_count++;

	gpio_drv_mode(pin, pullconfig == PULL_DOWN ? INPUT_PULLDOWN : pullconfig == PULL_UP ? INPUT_PULLUP : INPUT);

	config[handle].pin = pin;
	config[handle].active_on = active_level;
	config[handle].registered = true;

	ctx[handle].state = SW_STATE_IDLE;
	ctx[handle].timer = tim;

	return (sw_handle_t) handle;
}

void switch_drv_unregister(sw_handle_t handle) {
	if (handle < 0 || handle >= SW_MAX_SWS)
		return;
	timer_drv_stop(ctx[handle].timer);
	timer_drv_delete(ctx[handle].timer);
	config[handle].registered = false;
}

swEvent switch_drv_pop_event(void) {
	swEvent ret = {.event_type = SW_EVENT_NONE, .swPin = 0};
	if (_head == _tail)
		return ret;
	ret = queue[_tail];
	_tail = (_tail + 1u) & (SW_MAX_PENDING_EVENTS - 1u);
	return ret;
}

pisrCallbackPtr_t switch_drv_PISR(void) {
	for (uint8_t i = 0; i < sw_count; i++)
		if (config[i].registered)
			_switch_drv_process_event(i);
}

static void _switch_drv_push_event(swEvent ev) {
	uint8_t next = (_head + 1u) & (SW_MAX_PENDING_EVENTS - 1u);
	if (next == _tail)
		return;
	queue[_head] = ev;
	_head = next;
}

/***
 *
 * STM FLOW:
Idle > Debounce : active signal? start the 20ms debounce timer
Debounce > Idle : signal drops before 20ms, just debounce ->reset
Debounce > Pressed : confirmed press, start 800ms long-click timer
Pressed > Released : if  released before 800ms, start 300ms double-click timer
Pressed > Wait release : 800ms expires while held , emit long click, wait for actual release
Released > Pressed 2 : second press inside 300ms window
Released > Idle : 300ms expires with no second press, emit click
Pressed 2 > Idle : released before 800ms , emit double click
Pressed 2 > Wait release ; held 800ms on second press → emit long click
Wait release > Idle ; button physically released, clean return to idle
 *
Problem: if i do a double click and second click is too long , it will be taken as a long click only -> @todo solve
by incrementing long click window to avoid triggering that case accidentally
 *
 */
static void _switch_drv_process_event(uint8_t i) {
	uint8_t current = gpio_drv_read(config[i].pin);
	bool active = ISACTIVE(current, config[i].active_on);
	swContext_t *c = &ctx[i];
	swEvent ev = {.swPin = config[i].pin, .event_type = SW_EVENT_NONE};

	switch (c->state) {
		case SW_STATE_IDLE:
			if (active) {
				timer_drv_start(c->timer, TIMER_MS2TICKS(DEBOUNCE_MS), TIM_MODE_SINGLESHOT, NULL);
				c->state = SW_STATE_DEBOUNCE;
			}
			break;

		case SW_STATE_DEBOUNCE:
			if (!active) {
				timer_drv_stop(c->timer);
				c->state = SW_STATE_IDLE;
			} else if (timer_drv_expired(c->timer)) {
				timer_drv_start(c->timer, TIMER_MS2TICKS(LONG_CLICK_MS), TIM_MODE_SINGLESHOT, NULL);
				c->state = SW_STATE_PRESSED;
			}
			break;

		case SW_STATE_PRESSED:
			if (!active) {
				timer_drv_start(c->timer, TIMER_MS2TICKS(DOUBLE_CLICK_MS), TIM_MODE_SINGLESHOT, NULL);
				c->state = SW_STATE_RELEASED;
			} else if (timer_drv_expired(c->timer)) {
				ev.event_type = SW_EVENT_LONG_CLICK;
				_switch_drv_push_event(ev);
				c->state = SW_STATE_WAIT_RELEASE;
			}
			break;

		case SW_STATE_RELEASED:
			if (active) {
				timer_drv_start(c->timer, TIMER_MS2TICKS(LONG_CLICK_MS), TIM_MODE_SINGLESHOT, NULL);
				c->state = SW_STATE_PRESSED_2;
			} else if (timer_drv_expired(c->timer)) {
				ev.event_type = SW_EVENT_CLICK;
				_switch_drv_push_event(ev);
				c->state = SW_STATE_IDLE;
			}
			break;

		case SW_STATE_PRESSED_2:
			if (!active) {
				ev.event_type = SW_EVENT_DOUBLE_CLICK;
				_switch_drv_push_event(ev);
				c->state = SW_STATE_IDLE;
			} else if (timer_drv_expired(c->timer)) {
				ev.event_type = SW_EVENT_LONG_CLICK;
				_switch_drv_push_event(ev);
				c->state = SW_STATE_WAIT_RELEASE;
			}
			break;

		case SW_STATE_WAIT_RELEASE:
			if (!active)
				c->state = SW_STATE_IDLE;
			break;
	}
}
