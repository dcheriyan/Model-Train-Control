#include "clock.h"
#include "tasks.h"
#include "rpi.h"
#include "interrupts.h"
#include "util.h"
#include "kernel.h"
#include "GUI.h"

#define NUM_LINES 2
#define CHAR_MASK 0xFF

static volatile uint32_t* U3DR = (uint32_t*) UART3_B;
static volatile uint32_t* U0DR = (uint32_t*) UART0_B;

void marklin_notifier_task() {
    char uart_server_name [14] = "Wuart_server"; //first W is to encode as WhoIs message 
    int uart_server_id;
    uart_server_id = WhoIs(uart_server_name);

    //Store Messages
    char marklin_notifier_msg[10] = "M notif";
    char reply_buffer[12] = {0};
    //char type = 0;
    char msg = 0;
    char rx_value = 0;
    int rx_status = 0;

    for(;;) {
        Send(uart_server_id, marklin_notifier_msg, 9, reply_buffer, 10);
        //uart_printf(CONSOLE, "\033[35m marklin notifier: Received Message \033[37m \r\n");

        if (reply_buffer[0] == 'R'){
            //clear buffer
            marklin_notifier_msg[8] = 0;
            rx_value = 0; 
            //uart_printf(CONSOLE, "\033[35m marklin rx Before Await \033[37m \r\n");
            rx_status = RX_Read_Marklin();
            rx_value = rx_status; 
            //Set buffer
            marklin_notifier_msg[8] = rx_value;
            marklin_notifier_msg[7] = 'R';

        }
        else if (reply_buffer[0] == 'T') {
            msg = reply_buffer[1];
            volatile uint32_t* U3_FR = (uint32_t*) (UART3_B + 0x18);

            if ((*(U3_FR) & 0x1) == 0){ //Check if busy
                //uart_printf(CONSOLE, "marklin tx Wait for clear CTS");
                AwaitEvent(CTS_EVENT); // Wait for transaction to finish / line to go back up
            }

            *U3DR = msg; // Write the message into the register

            marklin_notifier_msg[7] = 'T';
        }

        //reset everything
        msg = 0;
        memset(reply_buffer, 0, 12);
    }

}

void konsole_notifier_task() {
    char uart_server_name [14] = "Wuart_server"; //first W is to encode as WhoIs message 
    int uart_server_id;
    uart_server_id = WhoIs(uart_server_name);

    //Store Messages
    char konsole_notifier_msg[10] = "K notif";
    char reply_buffer[12] = {0};
    //char type = 0;
    char msg = 0;
    char rx_value = 0;

    for(;;) {
        Send(uart_server_id, konsole_notifier_msg, 9, reply_buffer, 10);
        //uart_printf(CONSOLE, "\033[35m konsole notifier: Received Message \033[37m \r\n");

        if (reply_buffer[0] == 'R'){
            //clear buffer
            konsole_notifier_msg[8] = 0;
            rx_value = 0; 
            AwaitEvent(RX_KONSOLE_EVENT);
            uint32_t ch = *(U0DR); 
            rx_value = ch&CHAR_MASK;
            //Set buffer
            konsole_notifier_msg[8] = rx_value;
            konsole_notifier_msg[7] = 'R';

        }
        else if (reply_buffer[0] == 'T') {
            msg = reply_buffer[1];
            
            uart_putc(CONSOLE, msg);
            konsole_notifier_msg[7] = 'T';
        }

        //reset everything
        msg = 0;
        memset(reply_buffer, 0, 12);
    }

}


typedef struct uart_client_info{
    int requester_id;
    char type;
    char msg;
} uart_client_info;

static void uart_queue_remove(uart_client_info* queue_head, int queue_length, int idx_remove){
    for(idx_remove; idx_remove < queue_length; idx_remove ++){
        if (idx_remove == (queue_length - 1)){
            queue_head[idx_remove].requester_id = 0;
            queue_head[idx_remove].type = 0;
            queue_head[idx_remove].msg = 0;

        }
        else {
            queue_head[idx_remove].requester_id = queue_head[idx_remove + 1].requester_id ;
            queue_head[idx_remove].type = queue_head[idx_remove + 1].type ;
            queue_head[idx_remove].msg = queue_head[idx_remove + 1].msg;
        }
    }
}

void uart_server_task() {
    char my_name [14] = "Ruart_server"; //first R is to encode as RegisterAs message 
    int register_status = RegisterAs(my_name);

    //Create Notifiers
    int konsole_notif_id = 0;
    konsole_notif_id = Create(2, konsole_notifier_task);
    uart_printf(CONSOLE, "Created konsole Notifier task at tid: %d\r\n", konsole_notif_id);
    int marklin_notif_id = 0;
    marklin_notif_id = Create(5, marklin_notifier_task);
    uart_printf(CONSOLE, "Created marklin Notifier task at tid: %d\r\n", marklin_notif_id);

    //Track Status
    int marklin_avail = -2;
    int konsole_avail = -2;
    struct uart_client_info marklin_clients[MAX_TASKS] = {0};
    int marklin_next_slot = 0;
    struct uart_client_info konsole_clients[MAX_TASKS] = {0};
    int konsole_next_slot = 0;

    int transaction_complete = 0;

    //Store Messages
    int sender_id = 0;
    int line = 0;
    char server_msg_buffer[102] = {0};
    char notif_msg_buffer[12] = {0};
    char client_msg_buffer[10] = {0};
    char default_reply[4] = "ack";
    char rx_reply;
    char type;

    for(;;) {
        Receive(&sender_id, server_msg_buffer, 102); //Receive

        //Clent sending a Putc message
        if (server_msg_buffer[0] == 'P') {
            //uart_printf(CONSOLE, "\033[35m uart Server: Received Put Message \033[37m \r\n");
            //load into storage
            if (server_msg_buffer[1] == CONSOLE){
                konsole_clients[konsole_next_slot].requester_id = sender_id;
                konsole_clients[konsole_next_slot].msg = server_msg_buffer[2];
                konsole_clients[konsole_next_slot].type = 'T';
                konsole_next_slot++;
                if (konsole_avail == -1){
                    //if available send the next in line's info to them
                    notif_msg_buffer[0] = konsole_clients[0].type;
                    notif_msg_buffer[1] = konsole_clients[0].msg;
                    Reply(konsole_notif_id, notif_msg_buffer, 12);
                    //Set the tx_avail to client index in storage array
                    konsole_avail = 0;
                }
            }
            else if (server_msg_buffer[1] == MARKLIN){
                marklin_clients[marklin_next_slot].requester_id = sender_id;
                marklin_clients[marklin_next_slot].msg = server_msg_buffer[2];
                marklin_clients[marklin_next_slot].type = 'T';
                marklin_next_slot++;
                if (marklin_avail == -1){
                    //if available send the next in line's info to them
                    notif_msg_buffer[0] = marklin_clients[0].type;
                    notif_msg_buffer[1] = marklin_clients[0].msg;
                    Reply(marklin_notif_id, notif_msg_buffer, 12);
                    //Set the tx_avail to client index in storage array
                    marklin_avail = 0;
                }

            }
//            uart_printf(CONSOLE, "\033[35m uart Server: received line is: %d \033[37m \r\n", tx_clients[tx_next_slot].line);
//            uart_printf(CONSOLE, "\033[35m uart Server: received message is: %x \033[37m \r\n", tx_clients[tx_next_slot].msg );
        }
        //Clent sending a Puts message
        if (server_msg_buffer[0] == 'S') {
            //uart_printf(CONSOLE, "\033[35m uart Server: Received Puts Message \033[37m \r\n");
            //load into storage
            if (server_msg_buffer[1] == CONSOLE){
                
                int i = 2;
                uart_puts(CONSOLE, server_msg_buffer+2);
                konsole_clients[konsole_next_slot].requester_id = sender_id;
                konsole_clients[konsole_next_slot].msg = '\n';
                konsole_clients[konsole_next_slot].type = 'T';
                konsole_next_slot++;
                if (konsole_avail == -1){
                    //if available send the next in line's info to them
                    notif_msg_buffer[0] = konsole_clients[0].type;
                    notif_msg_buffer[1] = konsole_clients[0].msg;
                    Reply(konsole_notif_id, notif_msg_buffer, 12);
                    //Set the tx_avail to client index in storage array
                    konsole_avail = 0;
                }
            }
            else if (server_msg_buffer[1] == MARKLIN){
                int i = 2;
                for (; server_msg_buffer[i+1] != 255; i++) {
                    marklin_clients[marklin_next_slot].requester_id = 0; //only the last msg should trigger a reply
                    marklin_clients[marklin_next_slot].msg = server_msg_buffer[i];
                    marklin_clients[marklin_next_slot].type = 'T';
                    //uart_printf(CONSOLE, "\033[35m uart Server: Inserting %d into Marklin queue \033[37m \r\n", marklin_clients[marklin_next_slot].msg);
                    marklin_next_slot++;
                }
                marklin_clients[marklin_next_slot].requester_id = sender_id;
                marklin_clients[marklin_next_slot].msg = server_msg_buffer[i];
                marklin_clients[marklin_next_slot].type = 'T';
                //uart_printf(CONSOLE, "\033[35m uart Server: Inserting %d into Marklin queue \033[37m \r\n", marklin_clients[marklin_next_slot].msg);
                marklin_next_slot++;
                if (marklin_avail == -1){
                    //if available send the next in line's info to them
                    notif_msg_buffer[0] = marklin_clients[0].type;
                    notif_msg_buffer[1] = marklin_clients[0].msg;
                    Reply(marklin_notif_id, notif_msg_buffer, 12);
                    //Set the tx_avail to client index in storage array
                    marklin_avail = 0;
                }

            }
//            uart_printf(CONSOLE, "\033[35m uart Server: received line is: %d \033[37m \r\n", tx_clients[tx_next_slot].line);
//            uart_printf(CONSOLE, "\033[35m uart Server: received message is: %x \033[37m \r\n", tx_clients[tx_next_slot].msg );
        }
        //Client sending a Get message
        else if (server_msg_buffer[0] == 'G') {
            //uart_printf(CONSOLE, "\033[35m uart Server: Received Get Message \033[37m \r\n");
            //load into storage
            if (server_msg_buffer[1] == CONSOLE){
                konsole_clients[konsole_next_slot].requester_id = sender_id;
                konsole_clients[konsole_next_slot].msg = server_msg_buffer[2];
                konsole_clients[konsole_next_slot].type = 'R';
                konsole_next_slot++;
                if (konsole_avail == -1){
                    //if available send the next in line's info to them
                    notif_msg_buffer[0] = konsole_clients[0].type;
                    notif_msg_buffer[1] = konsole_clients[0].msg;
                    Reply(konsole_notif_id, notif_msg_buffer, 12);
                    //Set the tx_avail to client index in storage array
                    konsole_avail = 0;
                }
            }
            else if (server_msg_buffer[1] == MARKLIN){
                marklin_clients[marklin_next_slot].requester_id = sender_id;
                marklin_clients[marklin_next_slot].msg = server_msg_buffer[2];
                marklin_clients[marklin_next_slot].type = 'R';
                marklin_next_slot++;
                if (marklin_avail == -1){
                    //if available send the next in line's info to them
                    notif_msg_buffer[0] = marklin_clients[0].type;
                    notif_msg_buffer[1] = marklin_clients[0].msg;
                    Reply(marklin_notif_id, notif_msg_buffer, 12);
                    //Set the tx_avail to client index in storage array
                    marklin_avail = 0;
                }

            }

        }
        //Marklin Notifier is ready to receive a message
        else if (server_msg_buffer[0] == 'M') {
            //check if actually performed a query
            if (marklin_avail == -2) {
                marklin_avail = -1; //initialization
                uart_printf(CONSOLE, "marklin started \r\n");
            }
            else {
            //if performed query, reply to client
                if (server_msg_buffer[7] == 'R') {
                    rx_reply = server_msg_buffer[8]; // received char received on this
                    client_msg_buffer[0] = rx_reply;
                    Reply(marklin_clients[0].requester_id, client_msg_buffer, 10);
                    uart_queue_remove(marklin_clients, MAX_TASKS, 0);
                    marklin_next_slot--;

                }

                else if (server_msg_buffer[7] == 'T'){
                    //uart_printf(CONSOLE, "marklin tx finished, replying to %d \r\n", marklin_clients[0].requester_id);
                    //if performed query, reply to client
                    if (marklin_clients[0].requester_id != 0) {
                        Reply(marklin_clients[0].requester_id, default_reply, 4);
                    }
                    uart_queue_remove(marklin_clients, MAX_TASKS, 0);
                    marklin_next_slot--;
                    //uart_printf(CONSOLE, "finished removing");
                }

                //check if nonempty queue of waiter
                if (marklin_clients[0].type != 0) {
                    //if available send the next in line's info to them
                     //if available send the next in line's info to them
                    notif_msg_buffer[0] = marklin_clients[0].type;
                    notif_msg_buffer[1] = marklin_clients[0].msg;
                    Reply(marklin_notif_id, notif_msg_buffer, 12);
                    //Set the tx_avail to client index in storage array
                    marklin_avail = 0;
                }
                else {
                    //otherwise reset client idx tracker
                    marklin_avail = -1;
                }
            }
        }
        //TX Notifier is ready to send a message
        else if (server_msg_buffer[0] == 'K') {
            //check if actually performed a query
            if (konsole_avail == -2) {
                konsole_avail = -1; //initialization
                uart_printf(CONSOLE, "konsole started \r\n");
            }
            else {
            //if performed query, reply to client
                if (server_msg_buffer[7] == 'R') {
                    rx_reply = server_msg_buffer[8]; // received char received on this
                    client_msg_buffer[0] = rx_reply;
                    Reply(konsole_clients[0].requester_id, client_msg_buffer, 10);
                    uart_queue_remove(konsole_clients, MAX_TASKS, 0);
                    konsole_next_slot--;

                }

                else if (server_msg_buffer[7] == 'T'){
                    //uart_printf(CONSOLE, "konsole tx finished, replying to %d \r\n", konsole_clients[0].requester_id);
                    //if performed query, reply to client
                    Reply(konsole_clients[0].requester_id, default_reply, 4);
                    uart_queue_remove(konsole_clients, MAX_TASKS, 0);
                    konsole_next_slot--;
                    //uart_printf(CONSOLE, "finished removing");
                }

                //check if nonempty queue of waiter
                if (konsole_clients[0].requester_id > 0) {
                    //if available send the next in line's info to them
                     //if available send the next in line's info to them
                    notif_msg_buffer[0] = konsole_clients[0].type;
                    notif_msg_buffer[1] = konsole_clients[0].msg;
                    Reply(konsole_notif_id, notif_msg_buffer, 12);
                    //Set the tx_avail to client index in storage array
                    konsole_avail = 0;
                }
                else {
                    //otherwise reset client idx tracker
                    konsole_avail = -1;
                }
            }

        }

        memset(client_msg_buffer, 0, 10);        
        memset(notif_msg_buffer, 0, 12);
        memset(server_msg_buffer, 0, 20);

    }
}

void console_server_task() {
    char my_name [20] = "Rconsole_server"; //first R is to encode as RegisterAs message 
    int register_status = RegisterAs(my_name);
    uart_printf(CONSOLE, "Console Server Registered my name \r\n");
    
    //Track Status
    int console_avail = -2;
    struct uart_client_info console_clients[MAX_TASKS] = {0};
    int console_next_slot = 0;

    int transaction_complete = 0;

    char user_sensors[NUM_GOAL_SENSORS] = {0};
    char cpu_sensors[NUM_GOAL_SENSORS] = {0};

    //Store Messages
    int sender_id = 0;
    int line = 0;
    char server_msg_buffer[102] = {0};
    char notif_msg_buffer[12] = {0};
    char client_msg_buffer[10] = {0};
    char default_reply[4] = "ack";
    char rx_reply;
    char type;

    int sensor_triggered;
    int train_id;

    for(;;) {
        Receive(&sender_id, server_msg_buffer, 102); //Receive

        if (server_msg_buffer[0] == 'S') { // Print a String
            uart_puts(CONSOLE, server_msg_buffer+2);
            uart_puts(CONSOLE, "\r\n");
            Reply(sender_id, default_reply, 4);
        }
        else if (server_msg_buffer[0] == 'T') { // Update the Train positions and their triggered sensors
            sensor_triggered = server_msg_buffer[1];
            train_id = server_msg_buffer[2];
            Update_sensors_GUI(sensor_triggered, train_id, user_sensors, cpu_sensors);

            Reply(sender_id, default_reply, 4);
        }

        memset(client_msg_buffer, 0, 10);        
        memset(notif_msg_buffer, 0, 12);
        memset(server_msg_buffer, 0, 20);
    }
}

