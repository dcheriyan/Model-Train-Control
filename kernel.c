#include "kernel.h"
#include "clock.h"
#include "rpi.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "name_server.h"
#include "interrupts.h"
#include <sys/types.h>

/******* TASKS *******/ 
//"Global" Structures for keeping track of tasks 
// Don't trust the initialization here
static volatile uint32_t TID_counter = 0;
static volatile int current_tid = 0;
static volatile uint32_t current_mmap_idx = 0;
volatile struct Task_info mmap[MAX_TASKS];
static volatile int Event_tracker [NUM_EVENTS];
volatile int setInterrupt = 0;
static volatile uint32_t initTime = 0;
static volatile uint64_t idleTime = 0;
static volatile uint32_t idleEntryTime = 0;
static volatile uint32_t idleExitTime = 0;

#define RX_Buffer_M_LEN 15
static volatile char RX_Buffer_M[RX_Buffer_M_LEN]; //should only ever store 10 bytes, but we'll leave some extra room
static volatile int RX_Buffer_M_idx = 0;

static volatile int U3State = 1; // Should be clear to send at first time
static volatile int trainState = 0; // Start with lights off
static volatile int byteState = 0;
/******* PRIORITY *******/ 
// Lower priority value is more urgent
// We use clz (count leading 0s) to determine the queue status
// The first 56 bits should always be 0
// If clz = 0, then output the 0-th queue (left to right)
// If clz = 8 then nothing is in queue
static volatile uint64_t task_queue_indicator = 0;
// task_queues[priority][position]
static uint32_t task_queues[NUMBER_PRIORITIES][MAX_TASKS];

int hasRXInterrupt, hasCTSInterrupt;


/******* messages?? *******/ 
#define MQLength 2*MAX_TASKS
// Priority is in range [0, 7]

/****************** PRIVATE FUNCTIONS ************************/ 
/*************  General Use  **********/
int get_next_memory() {
    for (int i = 0; i < MAX_TASKS; i++){
        if (mmap[i].task_id == 0)
            return i;
    }
    uart_printf(CONSOLE, "You done messed up if you see me. \r\n");
    return -1; // You should not be getting this!
}

int get_task_index(int task_id) {
    if (task_id < 1) return -2; // Your input is invalid
    for (int i = 0; i < MAX_TASKS; i++){
        if (mmap[i].task_id == task_id && mmap[i].current_state != not_in_use) {
            return i;
        }
    }
    return -1; // Not found
}

int get_task_id(uint32_t mmap_idx) {
    return mmap[mmap_idx].task_id;
}

int queueInsert(int TID, int priority);

// returns tid
int32_t Create_Task(int priority, void (*function)()) {
    if (priority < 0 || priority > (NUMBER_PRIORITIES - 1)) return -1;

    // Set memory map
    int mmap_idx = get_next_memory();
    if (mmap_idx > -1) {
        mmap[mmap_idx].task_id = ++TID_counter;
        mmap[mmap_idx].current_state = ready;
        mmap[mmap_idx].priority = priority;
        mmap[mmap_idx].Stack_pointer = (STACK_BASE - STACK_SIZE * (uint64_t) mmap_idx);

        // Set Context
        mmap[mmap_idx].context.sp_el0_stored = mmap[mmap_idx].Stack_pointer;
        mmap[mmap_idx].context.elr_el1_stored = (uint64_t)function;
        mmap[mmap_idx].context.spsr_el1_stored = 0;
        for (int i = 0; i < NUMREG; i++) 
            mmap[mmap_idx].context.Registers[i] = 0;

        // For the idle_task only, since it should be the only one with priority 7
        if (mmap[mmap_idx].priority == 7){
            // Throw inital time into the register (every time)
            mmap[mmap_idx].context.Registers[0] = initTime;
        }

        // insert into Scheduling Queue
        queueInsert(TID_counter, priority);
        return TID_counter;
    }

    uart_printf(CONSOLE, "Task Descriptor is full!");
    return -2;
}

uint32_t Create_child_task(int priority, void (*function)()){
    int parent_tid = current_tid;
    int child_tid = Create_Task(priority, function);
    mmap[get_task_index(child_tid)].parent_id = parent_tid; 
    return child_tid;
}

void delete_Task(int tid){
    // Do some cleanup?
    int mmap_Tidx = get_task_index(tid);
    mmap[mmap_Tidx].current_state = not_in_use;
    mmap[mmap_Tidx].task_id = 0;
    mmap[mmap_Tidx].parent_id = 0;
    mmap[mmap_Tidx].priority = 0;
    mmap[mmap_Tidx].Stack_pointer = 0;
    mmap[mmap_Tidx].context.elr_el1_stored = 0;
    mmap[mmap_Tidx].context.sp_el0_stored = 0;
    mmap[mmap_Tidx].context.spsr_el1_stored = 0;
    for (int i = 0; i < NUMREG; i++) {
        mmap[mmap_Tidx].context.Registers[i] = 0;
    }
    for (int i = 0; i < MAX_TASKS; i++){
        mmap[mmap_Tidx].msg_tracker[i] = (Message_info){0,0,0,0,0};
        mmap[mmap_Tidx].receive_tracker = (Receive_info){0,0,0};
    }
}

/***************  Interrupts  ***************/
int Setup_interrupt(int cur_event_id, int awaiter_task_id) {
    switch (cur_event_id) {
        case TIMER_1_EVENT:
            Event_tracker[TIMER_1_EVENT] = awaiter_task_id;
            break;
        case TX_EVENT:
            Event_tracker[TX_EVENT] = awaiter_task_id;
            break;
        case CTS_EVENT:
                //uart_printf(CONSOLE, "\033[33m Waiting for CTS Interrupt %u \033[37m \r\n", awaiter_task_id);
                Event_tracker[CTS_EVENT] = awaiter_task_id;
                hasCTSInterrupt = 0;
            break;
        case RX_EVENT:
            //uart_printf(CONSOLE, "\033[33m Waiting for RX Interrupt %u \033[37m \r\n", awaiter_task_id);
            Event_tracker[RX_EVENT] = awaiter_task_id;
/*
            if (!hasRXInterrupt){
                uart_printf(CONSOLE, "\033[33m Waiting for RX Interrupt %u \033[37m \r\n", awaiter_task_id);
                Event_tracker[RX_EVENT] = awaiter_task_id;
                hasRXInterrupt = 0;

            }
            else {
                hasRXInterrupt = 0;
                return 0;
            }
*/
            break;
        case RX_KONSOLE_EVENT:
            //uart_printf(CONSOLE, "\033[33m Waiting for RX Interrupt on KONSOLE \033[37m \r\n");
            Event_tracker[RX_KONSOLE_EVENT] = awaiter_task_id;
            break;
        default:
            return -1;
    }
    return 1;
}

/***************  MessageQueues  ***************/
int messageQueueInsert(int tid, const char *msg, int msglen, char *reply, int rplen, int fromTID){
    int receiverIndex = get_task_index(tid);
    if (receiverIndex == -1) return -1; // You should really not be here! The code prior should have filtered you out already!

    for (int i = 0; i < MAX_TASKS; i++){
        // if sender is NULL then it is not used
        if (mmap[receiverIndex].msg_tracker[i].sender == NULL){
             mmap[receiverIndex].msg_tracker[i].sent_location = msg;
             mmap[receiverIndex].msg_tracker[i].sent_len = msglen;
             mmap[receiverIndex].msg_tracker[i].reply_location = reply;
             mmap[receiverIndex].msg_tracker[i].reply_len = rplen;
             mmap[receiverIndex].msg_tracker[i].sender = fromTID;
             return 0;
        }
    }
    return -1; // Ran out of space?
}

// returns idx on the MessageQueue
int messageQueueGet(int toTID, int fromTID){ 
    int toidx = get_task_index(toTID);
    for (int i = 0; i < MAX_TASKS && mmap[toidx].msg_tracker[i].sender != 0; i++){
        if (mmap[toidx].msg_tracker[i].sender == fromTID)
            return i;
    }
    return -1; // Does not exist!
}

// Set the message to be "sent"
// Note that it takes the index of the messageQueue
void messageQueueSet(int receiverID, int entryIdx){ 
    int receiverIndex = get_task_index(receiverID);

		// Just reset the sender locations is enough
    mmap[receiverIndex].msg_tracker[entryIdx].sent_location = NULL;    
    mmap[receiverIndex].msg_tracker[entryIdx].sent_len = 0;    
}

// Remove the message from queue
// Note that it takes the index of the messageQueue
void messageQueueClear(int receiverID, int entryIdx){ 
    int receiverIndex = get_task_index(receiverID);

    // We don't even need to do cleanup, we just overwrite this one by the one in front
    for (int i = entryIdx; i < MAX_TASKS && mmap[receiverIndex].msg_tracker[i].sender != 0; i++){
        mmap[receiverIndex].msg_tracker[i].sent_location = mmap[receiverIndex].msg_tracker[i+1].sent_location;    
        mmap[receiverIndex].msg_tracker[i].reply_location = mmap[receiverIndex].msg_tracker[i+1].reply_location;    
        mmap[receiverIndex].msg_tracker[i].sent_len = mmap[receiverIndex].msg_tracker[i+1].sent_len;    
        mmap[receiverIndex].msg_tracker[i].sender = mmap[receiverIndex].msg_tracker[i+1].sender;    
        mmap[receiverIndex].msg_tracker[i].reply_len = mmap[receiverIndex].msg_tracker[i+1].reply_len;    
    }

		// Set the last element to be empty
        mmap[receiverIndex].msg_tracker[MAX_TASKS-1].sent_location = NULL;   
        mmap[receiverIndex].msg_tracker[MAX_TASKS-1].reply_location = NULL;    
        mmap[receiverIndex].msg_tracker[MAX_TASKS-1].sent_len = 0;   
        mmap[receiverIndex].msg_tracker[MAX_TASKS-1].sender = 0; 
        mmap[receiverIndex].msg_tracker[MAX_TASKS-1].reply_len = 0;     
}

int receiverQueue(int* tid, char* msg, int msglen, int receiverID){
    int receiverIndex = get_task_index(receiverID);
    if (receiverIndex == -1) return -1; // Okay this should actually not happen. How did you even call a receiver dequeue without giving a valid receiver?!

     mmap[receiverIndex].receive_tracker.expected_sender = tid;
     mmap[receiverIndex].receive_tracker.receive_location = msg;
     mmap[receiverIndex].receive_tracker.receive_len = msglen;
    return 0;
}

void receiverClear(int receiverID){ 
    int receiverIndex = get_task_index(receiverID);

    mmap[receiverIndex].receive_tracker.expected_sender = NULL;    
    mmap[receiverIndex].receive_tracker.receive_location = NULL;    
    mmap[receiverIndex].receive_tracker.receive_len = 0;    
}

int deliverMsg(char* send_dest, char* recv_dest, int msglen, int bufferlen){
    memcpy(recv_dest, send_dest, msglen < bufferlen ? msglen : bufferlen);
    return msglen < bufferlen ? msglen : bufferlen; 
}

/***************  Scheduler  *****************/
// Function to set the kth bit of n
uint16_t setBit(uint16_t n, int k)
{
	return (n | (1 << (15-k)));
}

// Function to clear the kth bit of n
uint16_t clearBit(uint16_t n, int k)
{
	return (n & (~(1 << (15-k))));
}



void printQueue();

// Let Priority be [0, 16]
int queueInsert(int tid, int priority){
    // Hopefully each queue is enough to contain all possible tasks of that priority
    // But if it does not we will just try the queue with lower priority
    for (int i = priority; i < NUMBER_PRIORITIES; i++){
        for (int j = 0; j < MAX_TASKS; j++){
            if (task_queues[i][j] == 0){
                task_queues[i][j] = tid;

                // Set bit in bitvector to 1 to indicate the queue is not empty
                task_queue_indicator = setBit(task_queue_indicator, i);
                return 0;
            }
        }       
    }
    uart_printf(CONSOLE, "No more space in any queues!\n\r");
    return -1;
}

int queueInsertDefault(int tid){
    int priority = mmap[get_task_index(tid)].priority;
    return queueInsert(tid, priority);
}


int queueNPop(uint8_t n){
    // There should be something in the queue
    uint32_t taskID = task_queues[n][0];
    int returnID = 0;
    int retQIDX = 0;
    // Look in each queue starting from the highest order
    int q = n; // let q be the queue we look from
    for (; q < NUMBER_PRIORITIES; q++){
        // Look for each one in queue
        for (int i = 0; i < MAX_TASKS; i++){
            if (task_queues[q][i] == 0) break;
            taskID = task_queues[q][i];
            if (mmap[get_task_index(taskID)].current_state == running || mmap[get_task_index(taskID)].current_state == ready){
                returnID = taskID; // This is the return id
                retQIDX = i;	// This is the index of the queue to shift from
                break;
            }
        }

        if (returnID != 0) break;
    }

    if (returnID > 0){
        // shift the queue
        for(int i = retQIDX; i < MAX_TASKS - 1 && task_queues[q][i] != 0; i++){
            task_queues[q][i] = task_queues[q][i+1];
        }
        // Set the last entry of the queue to 0
        task_queues[q][MAX_TASKS - 1] = 0;
        // Reset the bit indicator
        if (task_queues[q][0] == 0){
            task_queue_indicator = clearBit(task_queue_indicator, q);
        }
        return returnID;
    }

    printQueue();
    // Why are you here? Why ARE YOU HERE?
    uart_printf(CONSOLE, "You're trying to pop an empty queue!, q is %d \r\n", q);
    //uart_printf(CONSOLE, "Idle should be at %d \r\n", task_queues[15][0]);
    return 0;
    
}

void printQueue(){
    //for (int i = 0; i < NUMBER_PRIORITIES; i++){
        /*
        uart_printf(CONSOLE, "Queue %u looks like: ", i);
        for (int j = 0; j < MAX_TASKS; j++){
            uart_printf(CONSOLE, "%u ", task_queues[i][j]);
        }
        uart_printf(CONSOLE, "\r\n");
        */
    //}
    //uart_printf(CONSOLE, "%u\n", task_queues[7][0]);
    //uart_printf(CONSOLE, "Indicator bitvector: %x\r\n", task_queue_indicator);
}

void printMSGQueues(int tid){
    int taskIdx = get_task_index(tid);
    uart_printf(CONSOLE, "Send Queue in Task : %u\r\n", tid);
    for (int j = 0; j < MAX_TASKS; j++){
        uart_printf(CONSOLE, "%u ", mmap[taskIdx].msg_tracker[j].sender);
    }
    uart_printf(CONSOLE, "\r\n");
}

// Basically choose which queue to pop from
// returns TID
int Schedule(){
    uint8_t num0s = 16;
    asm volatile ("clz %0, %1" : "=r" (num0s) : "r" (task_queue_indicator));
    num0s -= (64- NUMBER_PRIORITIES); // We are using only 16 out of 64 bits so subtract by 48
    if (num0s < NUMBER_PRIORITIES) {
        //uart_printf(CONSOLE, "Next tid has priority %d \r\n", num0s);
        return queueNPop(num0s);
    }
    // There is nothing in the queue anymore?!
    uart_printf(CONSOLE, "Nothing in the queue!\r\n");
    return -1;
}

/********* Context Switching ************/
// NOTE: THIS WILL DEFAULT SET TO RUN, OVERWRITE IT AFTER!
void store_context(int task_index){
    if (task_index != -1) {
        uint64_t register_val;
        //set state to ready
        mmap[task_index].current_state = ready;
        asm volatile("mrs %[zzz], sp_el0" : [zzz] "=r"(register_val));
        mmap[task_index].Stack_pointer = register_val;
        //Store General Registers
        for(int offset = 0; offset < NUMREG; offset++) {
            mmap[task_index].context.Registers[offset] = *((uint64_t *)(register_val - (32 - offset)*8));
        }
        //Store Special Registers
        mmap[task_index].context.sp_el0_stored = (register_val);
        asm volatile("mrs %[zzz], elr_el1" : [zzz] "=r"(register_val));
        mmap[task_index].context.elr_el1_stored = (register_val);
        // uart_printf(CONSOLE, "Stored PC: 0x%x \n\r", register_val);
        asm volatile("mrs %[zzz], spsr_el1" : [zzz] "=r"(register_val));
        mmap[task_index].context.spsr_el1_stored = (register_val);
    }
}

void printASM(){
    uart_printf(CONSOLE, "|-\n");
}

extern void restoreContext (volatile uint64_t* a1, uint64_t a2, uint64_t a3, uint64_t a4);

void restore_context(int task_index){
    restoreContext(mmap[task_index].context.Registers, mmap[task_index].context.sp_el0_stored, mmap[task_index].context.elr_el1_stored, mmap[task_index].context.spsr_el1_stored);
}

//keep assembly instructions separate from processing for ease of debugging
void Leave_kernel_for_task(int task_index){ 
    if (task_index != -1) {
        //set state to running
        mmap[task_index].current_state = running;
        
        // restore_context calls eret so this is always the last thing to do
        restore_context(task_index);
    }
}

void handle_request(uint32_t last_mmap_idx) { 
    //read esr_el1 to determine exception case
    int val;
    asm volatile("mrs %[zzz], esr_el1" : [zzz] "=r"(val));

    if (setInterrupt){
        store_context(last_mmap_idx);
        // Re-insert the current task into the Scheduler
        queueInsertDefault(current_tid);

        uint32_t interruptID = determine_interrupt_id();

        volatile uint32_t* U3DR = (uint32_t*) UART3_B;
        volatile uint32_t* U0DR = (uint32_t*) UART3_B;
        volatile uint32_t* U0_MIS = (uint32_t*) (UART0_B + 0x40);
        volatile uint32_t U0Status = 0;
        volatile uint32_t* U0_ICR = (uint32_t*) (UART0_B + 0x44);
        volatile uint32_t* U3_MIS = (uint32_t*) (UART3_B + 0x40);
        volatile uint32_t U3Status = 0;
        volatile uint32_t* U3_FR = (uint32_t*) (UART3_B + 0x18);
        volatile uint32_t* U3_ICR = (uint32_t*) (UART3_B + 0x44);
        // uart_printf(CONSOLE, "In Handler with ID %u", interruptID);
        switch (interruptID) {
            case TIMER_1_ID:
                timer_1_one_tick();
                // uart_printf(CONSOLE, "handling time");
                //unblock the task
                if (Event_tracker[TIMER_1_EVENT] != 0) {
                    int awaiter_idx = get_task_index(Event_tracker[TIMER_1_EVENT]);
                    mmap[awaiter_idx].context.Registers[0] = 0;
                    if (mmap[awaiter_idx].current_state != not_in_use) mmap[awaiter_idx].current_state = ready;
                    Event_tracker[TIMER_1_EVENT] = 0;
                }
                break;
            case UART_ID:
                // uart_printf(CONSOLE, "\033[33m UART Interrupt triggered \033[37m \r\n");
                U3Status = *(U3_MIS) & 0x7FF;
                U0Status = *(U0_MIS) & 0x7FF;
                //uart_printf(CONSOLE, "In UART with status %x\n", U0Status);
                if (U0Status & 0x10){
                    //uart_printf(CONSOLE, "Console Receive Interrupted!\n");
                    if (Event_tracker[RX_KONSOLE_EVENT] != 0) {
                        int awaiter_idx = get_task_index(Event_tracker[RX_KONSOLE_EVENT]);
                        mmap[awaiter_idx].context.Registers[0] = 0;
                        if (mmap[awaiter_idx].current_state != not_in_use) mmap[awaiter_idx].current_state = ready;
                        Event_tracker[RX_KONSOLE_EVENT] = 0;
                    }
                    
                    *U0_ICR |= 0x10;
                }
                if (U0Status & 0x20){ //Console Transmit (not used)
                    if (Event_tracker[TX_EVENT] != 0) {
                        int awaiter_idx = get_task_index(Event_tracker[TX_EVENT]);
                        mmap[awaiter_idx].context.Registers[0] = 0;
                        if (mmap[awaiter_idx].current_state != not_in_use) mmap[awaiter_idx].current_state = ready;
                        Event_tracker[TX_EVENT] = 0;
                    }
                    // uart_printf(CONSOLE, "\rTransmit Interrupted!\r");
                    *U0_ICR |= 0x20;
                }
                
                if (U3Status & 0x02){
                    //uart_printf(CONSOLE, "Marklin CTS Triggered!\n");

                    //uart_printf(CONSOLE, "UART3 FR is %u\n", *(U3_FR) & 0x1);
                    if ((Event_tracker[CTS_EVENT] != 0)) {
                        hasCTSInterrupt = 0;
                        int awaiter_idx = get_task_index(Event_tracker[CTS_EVENT]);
                        mmap[awaiter_idx].context.Registers[0] = 0;
                        if (mmap[awaiter_idx].current_state != not_in_use) mmap[awaiter_idx].current_state = ready;
                        Event_tracker[CTS_EVENT] = 0;
                    }
                    else
                        hasCTSInterrupt = 1;
                    *U3_ICR = 0x02;

       
                }
                if (U3Status & 0x10){
                    //uart_printf(CONSOLE, "Marklin Receive Interrupted!\r\n");
                    uint32_t ch = *U3DR;
                    //uart_printf(CONSOLE, "Marklin sent %x \r\n", ch);
                    if (Event_tracker[RX_EVENT] != 0) {
                        hasRXInterrupt = 0;
                        int awaiter_idx = get_task_index(Event_tracker[RX_EVENT]);
                        mmap[awaiter_idx].context.Registers[0] = ch;
                        if (mmap[awaiter_idx].current_state != not_in_use) mmap[awaiter_idx].current_state = ready;
                        Event_tracker[RX_EVENT] = 0;
                    }
                    else if (RX_Buffer_M_idx < (RX_Buffer_M_LEN -1)) {
                        RX_Buffer_M_idx ++;
                        RX_Buffer_M[RX_Buffer_M_idx] = ch;
                        //uart_printf(CONSOLE,"RX buffer stored %x \r\n", ch);

                    }

                    *U3_ICR |= 0x10;
                }
                if (U3Status & 0x20){
                    //Console Transmit (not ussed)
                    if (Event_tracker[TX_EVENT] != 0) {
                        int awaiter_idx = get_task_index(Event_tracker[TX_EVENT]);
                        mmap[awaiter_idx].context.Registers[0] = 0;
                        if (mmap[awaiter_idx].current_state != not_in_use) mmap[awaiter_idx].current_state = ready;
                        Event_tracker[TX_EVENT] = 0;
                    }
                    // uart_printf(CONSOLE, "\rTransmit Interrupted!\r");
                    *U3_ICR = 0x20;
                }

                break;
            default:
                uart_printf(CONSOLE, "Unknown interrupt with ID %u\n\r", interruptID);
        }
        clear_gic(interruptID);
        setInterrupt = 0;

        return;
    }

    switch (val&SVC_MASK) {
        case SVC_Yield: //Yield
            store_context(last_mmap_idx);
            // Re-insert the current task into the Scheduler
            queueInsertDefault(current_tid);
            break;
        case SVC_Create: // fork (create)
            store_context(last_mmap_idx);
            
            // Re-insert the current task into the Scheduler
            queueInsertDefault(current_tid);
            // param structure 
            // x1 is function ptr
            // x0 is priority
            int child_id = Create_child_task(mmap[last_mmap_idx].context.Registers[0], (void*) mmap[last_mmap_idx].context.Registers[1]);
            // return the child id by x0
            mmap[last_mmap_idx].context.Registers[0] = child_id;
            break;
        case SVC_Kill: // kill 
            delete_Task(current_tid);
            current_tid = 0;
            break;
        case SVC_getMyTid:
            // expect return value in x0
            store_context(last_mmap_idx);
            mmap[last_mmap_idx].context.Registers[0] = current_tid;
            // Re-insert the current task into the Scheduler
            queueInsertDefault(current_tid);
            break;
        case SVC_getParentTid:
            // expect return value in x0
            
            // Check if parent is still alive
            if (get_task_index(mmap[last_mmap_idx].parent_id) == -1){ 
                mmap[last_mmap_idx].parent_id = -1; // You're an orphan now.
            }
            store_context(last_mmap_idx);
            mmap[last_mmap_idx].context.Registers[0] = mmap[last_mmap_idx].parent_id;
            // Re-insert the current task into the Scheduler
            queueInsertDefault(current_tid);
            break;
        // Doing Scenario 1 for now
        case SVC_Send:

            store_context(last_mmap_idx);
            // Set it to sendwait
			mmap[get_task_index(current_tid)].current_state = sendwait;
            queueInsertDefault(current_tid);

            int from = current_tid;
            int to = mmap[last_mmap_idx].context.Registers[0];
            char* msgtext = (char*)mmap[last_mmap_idx].context.Registers[1];
            int msglen = mmap[last_mmap_idx].context.Registers[2];
            char* replyDest = (char*)mmap[last_mmap_idx].context.Registers[3];
            int replylen = mmap[last_mmap_idx].context.Registers[4];
            
            // invalid taskid to send to
            int receiverIndex = get_task_index(to);
            if (receiverIndex == -1){
                //return -1;
                mmap[get_task_index(current_tid)].context.Registers[0] = -1;
                break;
            }

			// If the receiver queue is already waiting for you
            if (mmap[receiverIndex].receive_tracker.expected_sender != NULL){

				// Deliver the message
                int recvRETVAL = deliverMsg(msgtext, mmap[receiverIndex].receive_tracker.receive_location, msglen, mmap[receiverIndex].receive_tracker.receive_len);
				// return this for receiver
                mmap[receiverIndex].context.Registers[0] = recvRETVAL;
                
				*(mmap[receiverIndex].receive_tracker.expected_sender) = current_tid;

                mmap[last_mmap_idx].current_state = replywait;
                mmap[receiverIndex].current_state = ready;
                receiverClear(to);
                messageQueueInsert(to, NULL, 0, replyDest, replylen, current_tid);
            }
            else{
                // Otherwise add to queue
                messageQueueInsert(to, msgtext, msglen, replyDest, replylen, current_tid);
                mmap[last_mmap_idx].current_state = sendwait;
            }

            break;

        case SVC_Recv:

            store_context(last_mmap_idx);
            mmap[get_task_index(current_tid)].current_state = recvwait;
            queueInsert(current_tid, mmap[last_mmap_idx].priority);

            int* expected_sender = (int*) (mmap[last_mmap_idx].context.Registers[0]);
            char* recv_dest = (char*) mmap[last_mmap_idx].context.Registers[1];
            int receive_len = mmap[last_mmap_idx].context.Registers[2];

            int waiter = 0;
            // if you already have a message in queue from a sender
            for (int i = 0; i < MAX_TASKS && mmap[last_mmap_idx].msg_tracker[i].sender != 0; i++) {
                if (mmap[last_mmap_idx].msg_tracker[i].sent_len != 0){
                    int recvRETVAL = deliverMsg(mmap[last_mmap_idx].msg_tracker[i].sent_location, recv_dest, mmap[last_mmap_idx].msg_tracker[i].sent_len, receive_len);
                    int sender = mmap[last_mmap_idx].msg_tracker[i].sender;
                    *expected_sender = sender;

                    messageQueueSet(current_tid, i); // Set it to be delivered
                    int senderIdx = get_task_index(sender);
                    mmap[senderIdx].current_state = replywait;
                    // Return this to receive()
                    mmap[last_mmap_idx].context.Registers[0] = recvRETVAL;
                    mmap[last_mmap_idx].current_state = ready;
                    waiter = 1;
                    break;
                }
            }
            if (waiter == 0) {
                // Put yourself into the waitqueue
                receiverQueue(expected_sender, recv_dest, receive_len, current_tid);
                mmap[last_mmap_idx].current_state = recvwait;
            }
            break;

        case SVC_Repl:

            store_context(last_mmap_idx);
            queueInsert(current_tid, mmap[last_mmap_idx].priority);
            int reply_tid = mmap[last_mmap_idx].context.Registers[0];
            char *receive_reply_location = (char*) mmap[last_mmap_idx].context.Registers[1];
            int receive_reply_len = mmap[last_mmap_idx].context.Registers[2];

			// Look for the message with the reply_tid
            int Qidx = messageQueueGet(current_tid, reply_tid);
            if (Qidx == -1){
                // DEBUG ERROR. What are you replying to?
                uart_printf(CONSOLE, "Replying to a non-existant task?! From %d, to %d \r\n",current_tid, reply_tid);
                mmap[last_mmap_idx].context.Registers[0] = -1;
                break;
            }
            if (mmap[get_task_index(reply_tid)].current_state == replywait){
                int sender_idx = get_task_index(reply_tid);
                int senderRETVAL = deliverMsg(receive_reply_location, mmap[last_mmap_idx].msg_tracker[Qidx].reply_location, receive_reply_len, mmap[last_mmap_idx].msg_tracker[Qidx].reply_len);

                // Return this to send()
                mmap[sender_idx].context.Registers[0] = senderRETVAL;

                // Return this to reply()
                mmap[last_mmap_idx].context.Registers[0] = senderRETVAL;
                mmap[sender_idx].current_state = ready;
                mmap[last_mmap_idx].current_state = ready;

                // receiver queue should already be clean
                // so clean up the message queue
                messageQueueClear(current_tid, Qidx);
            }
            else {
                // What? What are you replying to?
                uart_printf(CONSOLE, "Replying to a non-waiting task?!");
                mmap[last_mmap_idx].context.Registers[0] = -2;
            }
            break;

        case SVC_Await:
            store_context(last_mmap_idx);
            queueInsertDefault(current_tid);
            int eventid = mmap[last_mmap_idx].context.Registers[0];
            //uart_printf(CONSOLE, "Await with id %d \r\n", eventid);
            //handle interrupt request
            int interrupt_valid = Setup_interrupt(eventid, current_tid);
            if (interrupt_valid == 1) {
                //uart_printf(CONSOLE, "Interrupt is valid \r\n");
                mmap[last_mmap_idx].current_state = blocked;
            }
            else if (interrupt_valid == 0){
                mmap[last_mmap_idx].current_state = ready;
            }
            else {
                //don't block if invalid event
                uart_printf(CONSOLE, "Interrupt Invalid \r\n");
                mmap[last_mmap_idx].context.Registers[0] = -1;
            }
            break;
        case SVC_InitTime:
            // expect return value in x0
            store_context(last_mmap_idx);
            mmap[last_mmap_idx].context.Registers[0] = initTime;
            // Re-insert the current task into the Scheduler
            queueInsertDefault(current_tid);
            break;
        case SVC_IdleInq:
            // expect return value in x0
            store_context(last_mmap_idx);
            // Re-insert the current task into the Scheduler
            queueInsertDefault(current_tid);
            mmap[last_mmap_idx].context.Registers[0] = idleTime*100/(idleExitTime - initTime);
            break;
        case SVC_RX_Read_M:
            // expect return value in x0
            store_context(last_mmap_idx);
            queueInsertDefault(current_tid); // Re-insert the current task into the Scheduler
            if (RX_Buffer_M_idx > -1) {
                //uart_printf(CONSOLE,"\033[32m RX buffer has data %x \r\n \033[37m", RX_Buffer_M[0]);
                mmap[last_mmap_idx].context.Registers[0] = RX_Buffer_M[0];
                RX_Buffer_M[(RX_Buffer_M_LEN-1)] = 0;
                for (int i = 0; i < (RX_Buffer_M_LEN -2); i++){
                    RX_Buffer_M[i] = RX_Buffer_M[i+1]; //shift the queuedown
                }
                RX_Buffer_M_idx--;
            }
            else {
               // uart_puts(CONSOLE,"\033[32m RX buffer empty \r\n \033[37m");
                int interrupt_valid = Setup_interrupt(RX_EVENT, current_tid);  
                if (interrupt_valid == 1) {
                    //uart_printf(CONSOLE, "RX Event Interrupt was valid \r\n");
                    mmap[last_mmap_idx].current_state = blocked;
                }
                //mmap[last_mmap_idx].context.Registers[0] = -1;
            }
            
            break;

        default:
            uart_printf(CONSOLE, "Unknown SVC was valid \r\n");
    }

}

/******************  PUBLIC FUNCTIONS ****************/
int Kernel_initialize(int initial_task_priority, void (*initial_task_function)()){
    TID_counter = 0;
    current_tid = 0;
    current_mmap_idx = 0;
    task_queue_indicator = 0;
    setInterrupt = 0;

    initTime = get_current_time();
    idleTime = 0;
    idleEntryTime = 0;
    idleExitTime = initTime;

    trainState = 0;
    byteState = 0;

    RX_Buffer_M_idx = -1;
    for (int i = 0; i < RX_Buffer_M_LEN; i++){
        RX_Buffer_M[i] = 0;
    }


    for (int i = 0; i < NUM_INTERRUPTS; i++){
        Event_tracker[i] = 0;
    }
    
    // Setup TCBs and PQs
    for (int i = 0; i < MAX_TASKS; i++){
        mmap[i] = (Task_info) {0, 0, 0, 0, 0, {{0}, 0, 0 ,0}, {NULL, 0, 0, NULL, 0}, {0, NULL, 0}};  //TODO: Fix to work with messages
        for(int j = 0; j < NUMBER_PRIORITIES; j++){
            task_queues[j][i] = 0;
        }
    }

    // UART Initialization
    uart_init();
    uart_config_and_enable_console();
    uart_config_and_enable_marklin();

    hasCTSInterrupt = 0;
    hasRXInterrupt = 0;

    uart_puts(CONSOLE, "Starting Kernel \n\r");
    interrupts_init();
    //handle_timer_1();
    uart_puts(CONSOLE, "Finished Interrupts Init \n\r");

    // Create Name server at Tid 1, 1 priority
    // Since its the first task the create task function should give it Tid 1
    current_tid = Create_Task(5, Name_server_task); 
    // Warn us if an act of God has lead to Name server not being created at Tid 1
    if (mmap[get_task_index(NAME_SERVER_TID)].context.elr_el1_stored != Name_server_task) {
        uart_puts(CONSOLE, "Name server was not created at its expected TID! \n\r");
        uart_puts(CONSOLE, "Finishing initialization then exiting \n\r");
    }
    else {
        // Initialize Original TCB
        current_tid = Create_Task(initial_task_priority, initial_task_function); 
        // initial schedule
        current_tid = Schedule();
    }
    //Start the task
    //Note: if Name server did not get its TID it will immediately exit causing the Kernel to exit shortly thereafter
    current_mmap_idx = get_task_index(current_tid);
    Leave_kernel_for_task(current_mmap_idx);

    return 1;
}

int Kernel_loop() {
    // Permanent Stuff
    // Note the irq svc sends us back to where kernel loop starts instead of where it previously was
    // so we need to do handle_request first
    for(;;){
        
        //Tracking Idle Time
        if (mmap[current_mmap_idx].priority == 15){
            idleExitTime = get_current_time();
            if (idleEntryTime != 0){    //make sure idle task is running
                idleTime += (idleExitTime - idleEntryTime);
            }
        }
        

        //handle request
        handle_request(current_mmap_idx);

        // Schedule
        current_tid = Schedule();
        current_mmap_idx = get_task_index(current_tid);

        if (current_tid == -1){
            uart_printf(CONSOLE, "Kernel going to sleep. zzz\r\n");
            return 0;
        }

        // Tracking Idle Time
        if (mmap[current_mmap_idx].priority == 15){
            idleEntryTime = get_current_time();
        }

        //activate request
        Leave_kernel_for_task(current_mmap_idx);
    }
}
