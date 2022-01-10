#include <time/timer.h>
#include <video.h>
#include <ports.h>
#include <cpu/task.h>
#include <cpu.h>
#include <apic.h>

#define TIME_TIMER_PIT_BASE_HZ       1193181
#define TIME_TIMER_PIT_COMMAND_PORT  0x43
#define TIME_TIMER_PIT_COMMAND_WRITE 0x34
#define TIME_TIMER_PIT_DATA_PORT     0x40

__volatile__ uint64_t time_timer_tick_count = 0;

__volatile__ uint64_t time_timer_spinsleep_counter_value = 0;
__volatile__ uint8_t time_timer_start_spinsleep_counter = 0;

void time_timer_reset_tick_count() {
	time_timer_tick_count = 0;
}

int8_t time_timer_pit_isr(interrupt_frame_t* frame, uint8_t intnum){
	UNUSED(frame);
	UNUSED(intnum);

	time_timer_tick_count++;

	apic_eoi();
	cpu_sti();

	return 0;
}

void time_timer_pit_set_hz(uint16_t hz) {
	uint16_t divisor = TIME_TIMER_PIT_BASE_HZ / hz;         /* Calculate our divisor */
	outb(TIME_TIMER_PIT_COMMAND_PORT, TIME_TIMER_PIT_COMMAND_WRITE);               /* Set our command byte 0x36 */
	outb(TIME_TIMER_PIT_DATA_PORT, divisor & 0xFF);     /* Set low byte of divisor */
	outb(TIME_TIMER_PIT_DATA_PORT, divisor >> 8);       /* Set high byte of divisor */
}

void time_timer_pit_sleep(uint64_t usecs) {
	time_timer_reset_tick_count();
	while(time_timer_tick_count <= usecs);
}

int8_t time_timer_apic_isr(interrupt_frame_t* frame, uint8_t intnum) {
	UNUSED(frame);
	UNUSED(intnum);

	time_timer_tick_count++;

	if(time_timer_start_spinsleep_counter) {
		time_timer_start_spinsleep_counter = 0;
	}

	if((time_timer_tick_count % TASK_MAX_TICK_COUNT) == 0) {
		task_switch_task();
	}

	if((time_timer_tick_count % 1000) == 0) {
		PRINTLOG(TIMER, LOG_DEBUG, "timer hits!, value 0x%lx", time_timer_tick_count);
	}

	apic_eoi();
	cpu_sti();

	return 0;
}

uint64_t time_timer_get_tick_count() {
	return time_timer_tick_count;
}

void time_timer_configure_spinsleep() {
	time_timer_start_spinsleep_counter = 1;

	while(time_timer_start_spinsleep_counter) {
		time_timer_spinsleep_counter_value++;
	}

	PRINTLOG(TIMER, LOG_TRACE, "spinsleep counter is 0x%lx", time_timer_spinsleep_counter_value);
}

void time_timer_spinsleep(uint64_t usecs) {
	PRINTLOG(TIMER, LOG_TRACE, "spinsleep for 0x%lx", usecs);
	while(usecs--) {
		uint64_t spinsleep_counter = time_timer_spinsleep_counter_value;
		while(spinsleep_counter--);
	}
	PRINTLOG(TIMER, LOG_TRACE, "spinsleep finished", 0);
}
