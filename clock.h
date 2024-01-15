#ifndef _clock_h_
#define _clock_h_ 1

#include <stdint.h>

uint32_t get_current_time(); //True time based on number of ticks

void Clock_server_task();

void Clock_notifier_task();

#endif /* clock.h */