#ifndef _interrupts_h_
#define _interrupts_h_ 1

#include <stdint.h>

//Interrupt IDs
#define TIMER_1_ID 97
#define UART_ID 153
#define NUM_INTERRUPTS 2

//Event IDs
#define TIMER_1_EVENT 0
#define TX_EVENT 1
#define CTS_EVENT 2
#define RX_EVENT 3
#define RX_KONSOLE_EVENT 4
#define NUM_EVENTS 5

//Debugging
void initialize_timer();
void timer_1_sec();

// For Kernel
void interrupts_init();
uint32_t determine_interrupt_id();
void clear_gic(uint32_t interrupt_id);
void timer_1_one_tick();

#endif //interrupts.h
