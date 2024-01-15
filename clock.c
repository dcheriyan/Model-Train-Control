#include "clock.h"
#include "tasks.h"
#include "rpi.h"
#include "interrupts.h"
#include "util.h"
#include "kernel.h"

static const volatile uint32_t* CLOCK_REGISTER = (uint32_t*) (0xFE000000 + 0x3000 + 0x4);

uint32_t get_current_time() { 
    return *CLOCK_REGISTER;
}

void Clock_notifier_task() {
    char clock_server_name [14] = "Wclock_server"; //first W is to encode as WhoIs message 
    int clock_server_id;
    clock_server_id = WhoIs(clock_server_name);

    //Messages
    char notifier_msg[5] = "Tick";
    char reply_buffer[4] = {0};

    //Setup initial tick
    timer_1_one_tick();

    for(;;) {
        AwaitEvent(TIMER_1_EVENT);
        Send(clock_server_id, notifier_msg, 5, reply_buffer, 4);
        //Don't care about reply so don't bother resetting the mem to be slightly more efficient
    }

}

typedef struct Delay_info{
    int requester_id;
    int end_time;
} Delay_info;

static void queue_remove(Delay_info* queue_head, int queue_length, int idx_remove){
    for(idx_remove; idx_remove < queue_length; idx_remove ++){
        if (idx_remove == (queue_length - 1)){
            queue_head[idx_remove].requester_id = 0;
            queue_head[idx_remove].end_time = 0;
        }
        else {
            queue_head[idx_remove].requester_id = queue_head[idx_remove + 1].requester_id;
            queue_head[idx_remove].end_time = queue_head[idx_remove + 1].end_time;
        }
    }
}

void Clock_server_task() {
    char my_name [14] = "Rclock_server"; //first R is to encode as RegisterAs message 
    int register_status = RegisterAs(my_name);

    //Create Notifier
    int returned_task_id = 0;
    returned_task_id = Create(4, Clock_notifier_task);
    uart_printf(CONSOLE, "Created Clock Notifier task at tid: %d\r\n", returned_task_id);

    struct Delay_info delay_storage[MAX_TASKS] = {0};
    //Store Messages
    int sender_id = 0;
    char server_msg_buffer[20] = {0};
    char default_reply[4] = "ack";
    char Time_reply[10] = {0};
    char notifier_msg[5] = "Tick";
    char Time_msg[5] = "Time";
    char Delay_msg[6] = "D";
    char Delay_Until_msg[12] = "U";
    uint32_t start_time = get_current_time();
    int start_ticks = start_time/10000;
    //uart_printf(CONSOLE, "Server: Created @ %d \r\n", start_ticks);
    uint32_t raw_time = 0;
    int cur_ticks = 0;
    int msg_conversion = 0;
    int next_slot = 0;

    for(;;) {
        Receive(&sender_id, server_msg_buffer, 20); //Receive

        raw_time = get_current_time();
        cur_ticks = raw_time/10000;

        if (cust_strcmp(server_msg_buffer, notifier_msg) == 0){
            //uart_printf(CONSOLE, "Server: Received tick @ %d \r\n", raw_time);
            Reply( sender_id, default_reply, 4);
        }
        else if (cust_strcmp(server_msg_buffer, Time_msg) == 0){
            int* temp_cast = Time_reply;
            *temp_cast = (int) (cur_ticks - start_ticks);
            Reply(sender_id, Time_reply, 8);
        }
        else if (server_msg_buffer[0] == 'D') {
            //uart_printf(CONSOLE, "Server: Received delay @ %d \r\n", (cur_ticks - start_ticks));
            delay_storage[next_slot].requester_id = sender_id;
            msg_conversion = server_msg_buffer[1]*(255^3) + server_msg_buffer[2]*(255^2) + server_msg_buffer[3]*(255) + server_msg_buffer[4];
            //uart_printf(CONSOLE, "Server: Will delay for %d \r\n", msg_conversion);
            delay_storage[next_slot].end_time = cur_ticks + msg_conversion;
            next_slot++;
        }
        else if (server_msg_buffer[0] == 'U') {
            //uart_printf(CONSOLE, "Server: Received delay Until @ %d \r\n", (cur_ticks - start_ticks));
            msg_conversion = server_msg_buffer[1]*(255^3) + server_msg_buffer[2]*(255^2) + server_msg_buffer[3]*(255) + server_msg_buffer[4];
            if (msg_conversion < (cur_ticks - start_ticks)){
                int* temp_cast = Time_reply;
                *temp_cast = (int) (-2);
                Reply(sender_id, Time_reply, 8);
            }
            else {
                delay_storage[next_slot].requester_id = sender_id;
                delay_storage[next_slot].end_time = msg_conversion + start_ticks;
                next_slot++;
            }
        }
        
        for(int i = 0; i < MAX_TASKS; i++){
            if (delay_storage[i].requester_id == 0){
                break;
            }
            if (delay_storage[i].end_time <= cur_ticks){
                int* temp_cast = Time_reply;
                *temp_cast = (int) (cur_ticks - start_ticks);
                Reply(delay_storage[i].requester_id, Time_reply, 8);
                queue_remove(delay_storage, MAX_TASKS, i);
                i--;
                next_slot--;
            }

        }

        memset(Time_reply, 0, 10);
        memset(server_msg_buffer, 0, 20);

    }
}
