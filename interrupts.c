#include "interrupts.h"
#include "rpi.h"
#include <stdint.h>

static char* const  MMIO_BASE = (char*) 0xFE000000;
static char* const SYS_TIMER_BASE = (char*)(MMIO_BASE + 0x3000);
static volatile uint32_t* const CS_REGISTER = (uint32_t*) SYS_TIMER_BASE;
static const volatile uint32_t* const CLO_REGISTER = (uint32_t*) (SYS_TIMER_BASE + 0x4);
static volatile uint32_t* const C1_REGISTER = (uint32_t*) (SYS_TIMER_BASE + 0x10);

static char* const GIC_BASE = (char*) 0xFF840000;
static char* const GICD_BASE = (char*) (GIC_BASE + 0x1000);
static char* const GICD_ISENABLER = (char*) (GICD_BASE + 0x100);
static char* const GICD_ITARGETSR = (char*) (GICD_BASE + 0x800);

static char* const GICC_BASE = (char*) (GIC_BASE + 0x2000);
static volatile uint32_t* const GICC_EOIR = (uint32_t *)(GICC_BASE + 0x10);

// Setup an interrupt based on the docs
void interrupt_enable(int id) {
    volatile uint32_t* enablerN = (uint32_t*) ((id/32)*4 + 0x100 + GICD_BASE); 
    *enablerN = (*enablerN) | (uint32_t) 1 << (id % 32);

    volatile uint32_t* targetN = (uint32_t*) ((id/4)*4 + 0x800 + GICD_BASE);
    // Set the corresponding byte offset to 1 to refer to core 0
    *targetN = (*targetN) | (uint32_t) (1 << ((id%4) * 8)); 
}

void interrupts_init() {
    interrupt_enable(TIMER_1_ID); 
    interrupt_enable(UART_ID); 
    volatile uint32_t *UART0_IMSC = (uint32_t*) (UART0_B + 0x38);
    volatile uint32_t *UART3_IMSC = (uint32_t*) (UART3_B + 0x38);
    volatile uint32_t *UART3_DR = (uint32_t*) (UART3_B);
    *(UART0_IMSC) = 0x30;
    *(UART3_IMSC) = 0x12;
    *UART3_DR = 0x20;
    
    uart_printf(1, "Enabled Interrupt \r\n");
}

uint32_t determine_interrupt_id() {
    volatile uint32_t interruptVal = *((uint32_t*)(GICC_BASE + 0x000c));      
    uint32_t interruptID = interruptVal & 0x1FF;

    //uart_printf(1, "Interrupt ID is : %u\r\n", interruptID);

    return interruptID;
}

void clear_gic(uint32_t interrupt_id) {
    *GICC_EOIR = interrupt_id;
}

void timer_1_one_tick() {
    *CS_REGISTER |= (uint32_t) 0x2;
    *C1_REGISTER = (uint32_t) *CLO_REGISTER + 10000; //1 tick = 10,000 'true' ticks
}
