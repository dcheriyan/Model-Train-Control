#ifndef _kernel_h_
#define _kernel_h_ 1

#include <stdint.h>

//Stack information
#define STACK_BASE 0x4000000
#define STACK_SIZE 0x100000
//#define END_OF_STACK_BASE 0x3000000

//SVC Information
#define SVC_MASK 0xFFFFFF //only lower 24 bits have exception code info
#define SVC_Yield 1
#define SVC_Create 2
#define SVC_Kill 3
#define SVC_getMyTid 4
#define SVC_getParentTid 5
#define SVC_Send 6
#define SVC_Recv 7
#define SVC_Repl 8
#define SVC_Await 9
#define SVC_InitTime 10
#define SVC_IdleInq 11
#define SVC_RX_Read_M 12

//General things
#define MAX_TASKS 16
#define NUMBER_PRIORITIES 16 //Number of Priorities
#define NUMREG 31
#define NAME_SERVER_TID 1 //Always the first task created

#define MAX_BUFFERLEN 256

//Task Storage Structures
enum State {
    not_in_use,
    running,
    ready,
    sendwait,
    recvwait,
    replywait,
    blocked,
    await_blocked
};

typedef struct Task_Storage {
    uint64_t Registers[NUMREG];
    uint64_t sp_el0_stored;
    uint64_t elr_el1_stored;
    uint64_t spsr_el1_stored;
} Task_Storage;

typedef struct Message_info{
    char* sent_location;
    int sent_len;
    int sender;
    char* reply_location;
    int reply_len;
} Message_info;

typedef struct Receive_info{
    int* expected_sender;
    char* receive_location;
    int receive_len;
} Receive_info;

typedef struct Task_info {
  int task_id;
  int parent_id;
  uint8_t priority;
  enum State current_state;
  uint64_t Stack_pointer;
  struct Task_Storage context;

  struct Message_info msg_tracker[MAX_TASKS];
  struct Receive_info receive_tracker;
} Task_info;

int Kernel_initialize(int initial_task_priority, void (*initial_task_function)());

int Kernel_loop();

#endif
