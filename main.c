#include "rpi.h"
#include "tasks.h"
#include "kernel.h"
#include "tasks.h"
#include <stdint.h>
#include <stdbool.h>

#include "interrupts.h"
#include "clock.h"
#include "uart_server.h"
#include "a0.h"
#include "util.h"
#include "track.h"
#include "GUI.h"

extern struct Train_info Known_Trains[NUM_CHAR_TRAIN];

int direction, offset;
struct track_node Track_B[TRACK_MAX];
char switch_data[NUM_SWITCHES][2];
int track_input;

void setDirection(int sensor){
    if (sensor == 45){
        direction = 1;
        offset = 0; 
        return;
    }
    if (sensor == 70){
        direction = 0;
        offset = 120;
        return;
    }
}

void Idle_task(){
    volatile uint32_t init_time = getInitTime();

    volatile int idle_usage = 0;
    // uart_printf(CONSOLE, "Starting Idle Task \r\n");
    volatile uint32_t idle_time = 0;
    volatile uint32_t cur_time = 0;
    volatile uint32_t prev_time = get_current_time();
    volatile uint32_t time_active = init_time - get_current_time();
    volatile int idle_counter = 0;

    volatile uint32_t *UART3_IMSC = (uint32_t*) (UART3_B + 0x38);
    volatile uint32_t *UART3_DR = (uint32_t*) (UART3_B);

    for(;;) {
        cur_time = get_current_time();
        if (cur_time > (prev_time + 10000000)){ //Update Every 10 secs
            idle_usage = Idle_time_inq();
            //uart_printf(CONSOLE, "\r\n Idling %u %% of the time! \r\n", idle_usage);
            prev_time = cur_time;
        }
        
    }
    Exit();
}

void Display_Clock(){
    char clock_server_name [14] = "Wclock_server"; //first W is to encode as WhoIs message 
    int clock_server_id;
    clock_server_id = WhoIs(clock_server_name);

    //Data strucutres to track time
    uint32_t UI_start_time = get_current_time();
    uint32_t timer_data[4] = {0,0,0,0};
    int delay_interval = 9;

    for (;;){
        Delay(clock_server_id, delay_interval);
        update_display_time_value(UI_start_time, timer_data);
        print_formatted_time_line(timer_data);
    }

    Exit();
}

void Query_Sensors(){
    uart_printf(CONSOLE, "Query Sensor Starting ... \r\n");

    char register_sensor [14] = "Rsensor"; //first R is to encode as RegisterAs message 
    int register_sesnor_status = RegisterAs(register_sensor);

    char uart_server_name [14] = "Wuart_server"; //first W is to encode as WhoIs message 
    int uart_server_id;
    uart_printf(CONSOLE, "Sensor Query: querying for uart server id \r\n");
    uart_server_id = WhoIs(uart_server_name);

    char clock_server_name[14] = "Wclock_server"; //first W is to encode as WhoIs message 
    int clock_server_id;
    uart_printf(CONSOLE, "Sensor Query: querying for clock server id \r\n");
    clock_server_id = WhoIs(clock_server_name);
    int ctrl_id = MyParentTid(); 

    int received = 0;
    char sensor_time_msg[10] = {0};
    uint32_t *raw_sensor_time = (uint32_t *)sensor_time_msg;
    char reply_buffer[4] = {0};
    char msg_buffer[4] = {0};
    char sensor_54_buffer[4] = {0};
    char sensor_77_buffer[4] = {0};
    char blacklist_sensor_buffer[4] = {0}; //incase a train is sitting on a switch when it stops
    char go_msg[3] = {'G', 'O', 0};
    int sender_id = 0;

    int test_status = 1;

    //Sensor data
    uint8_t sensor_loop_state = 0;
    uint8_t sensor_data = 0;
    char triggered_sensor_buffer[NUM_RECENT_SENSOR][4] = {{' ', ' ', ' ', '\0'},{' ', ' ', ' ', '\0'},{' ', ' ', ' ', '\0'},{' ', ' ', ' ', '\0'},{' ', ' ', ' ', '\0'}};

    Putc(uart_server_id, MARKLIN, 192); //Reset Mode on

    uint32_t start = 0;
    uint32_t end = 0;
    uint32_t total = 0;
    uint32_t new_total = 0;
    int desire_sensor_triggered_54 = 0;
    int desire_sensor_triggered_77 = 0;

    int loop_count = 0;
    uart_printf(CONSOLE, "Sensor query sending to crtl id %d \r\n", ctrl_id);

    Send(ctrl_id, sensor_time_msg, 10, reply_buffer, 4); //make sure tasks are registersd
    int test_case = 0;

    //Get Test Case (Which trains are being used)
    switch(reply_buffer[0]){
        case 'a':
            test_case = 3;
            break;
        case 'b':
            test_case = 2;
            break;
        case 'c':
            test_case = 7;
            break;
    }

    char name_54 [14] = "Wtrain_54"; //first W is to encode as WhoIs message 
    char name_77 [14] = "Wtrain_77"; //first W is to encode as WhoIs message 
    int train_77_id = 0;
    int train_54_id = 0;

    if (test_case%2)
        train_54_id = WhoIs(name_54);
    if (test_case%3)
        train_77_id = WhoIs(name_77);

    uart_printf(CONSOLE, "Sensor query Train 54 found id %d \r\n", train_54_id);
    uart_printf(CONSOLE, "Sensor query Train 77 found id %d \r\n", train_77_id);

    //States : 'r' ready, 'l' locked out, 'n' polling for next senor, 's' polling / searching for specific sensor, 'd' done with a route, 'c' confirm safe to start, x is telling sensor done, i is ignore, p is planning.
    char status_77 = 'r';
    char status_54 = 'r';

    uint32_t elapse_time_54 = 0;
    uint32_t elapse_time_77 = 0;
    uint32_t wait_54_time = 0;
    uint32_t wait_77_time = 0;

    while(test_status == 1){  

        // Train 54 has a delay
        if (status_54 == 'i') {
            while (!(test_case%3) && status_54 == 'i') { // busy loop if other train is disabled
                if (get_current_time() >= wait_54_time){
                    status_54 = 'r';
                }
            }
            if (get_current_time() >= wait_54_time){
                status_54 = 'r';
                Reply(train_54_id, go_msg, 3);
            }
        }

        // Train 77 has a delay
        if (status_77 == 'i') {
            while (!(test_case%2) && status_77 == 'i') { // busy loop if other train is disabled
                if (get_current_time() >= wait_77_time) { 
                    status_77 = 'r';
                }
            }
            if (get_current_time() >= wait_77_time) {
                status_77 = 'r';
                Reply(train_77_id, go_msg, 3);
            }
        } 
       
        //cases: make sure 54 enabled             make sure 77 enabled                  both pause                      make sure 54 paused or done (always expect another message from 77) and 77 done or disabled      77 paused or done and 54 done or disabled       
        if ((test_case%2 && status_54 == 'r')|| (test_case%3 && status_77 == 'r') || (status_54 == 'p' && status_77 == 'p')|| ((status_54 == 'p'|| status_54 == 'd') && (!(test_case%3) || status_77 == 'd')) || ((!(test_case%2) || status_54 == 'd') && status_77 == 'p')){
            Receive(&sender_id, msg_buffer, 4); //
            if (sender_id == train_54_id){
                if (status_54 != 'l') { //only allowed to process nessage if not locked
                    if (msg_buffer[0] == 'c') { //preparing to localize. Only one train can run
                        //uart_printf(CONSOLE, "Sensor query Train 54 confirm\r\n");
                        status_77 = 'l'; //lock 77 from request
                        status_54 = 'c';
                    }
                    else if (msg_buffer[0] == 'n') { //get next sensor / localize. Only one train can run
                        //uart_printf(CONSOLE, "Sensor query Train 54 next sensor\r\n");
                        status_77 = 'l'; //lock 77 from request
                        status_54 = 'n';
                    }
                    else if (msg_buffer[0] == 'f') { //finished localization. Unlock
                        Reply(train_54_id, go_msg, 3); //free the sender
                        //uart_printf(CONSOLE, "Sensor query Train 54 finished localization\r\n");
                        if (sensor_77_buffer[0] == 'c'){ //may have received an intermediate 'c' request from 77
                            status_77 = 'c'; 
                            status_54 = 'l';
                        }
                        else { //otherwise set both to ready
                            status_77 = 'r'; 
                            status_54 = 'r';
                        }
                    }
                    else if (msg_buffer[0] == 'd') { //Done with a Path
                        status_54 = 'd';
                        Reply(train_54_id, go_msg, 3); 
                    }
                    else if (msg_buffer[0] == 'p') { //train is replanning, ignore until its ready
                        status_54 = 'p';
                        Reply(train_54_id, go_msg, 3);
                    }
                    else if (msg_buffer[0] == 'r') { //train is finished replanning and ready to move again
                        status_54 = 'r';
                        Reply(train_54_id, go_msg, 3);
                    }
                    else if (msg_buffer[1] == 'R') { //skip MR / BR node messages that somehow get sent
                        status_54 = 'r';
                        Reply(train_54_id, go_msg, 3);
                    }
                    else if (msg_buffer[0] == 'F') { //Fake Sensor. Used to reach some specific locations
                        wait_54_time = get_current_time() + 1000000; //1 seconds
                        status_54 = 'i';
                        
                        //Reply(train_54_id, go_msg, 3);
                    }
                    else {
                        status_54 = 's'; //polling for a sensor value
                        if (elapse_time_54 != 0) {
                            //uart_printf(CONSOLE, "Time Waiting for 54 request: %d ms \r\n", (get_current_time() - elapse_time_54)/1000);
                        }
                    }
                }
                //uart_printf(CONSOLE, "54 sent server request: %s \r\n", msg_buffer);
                sensor_54_buffer[0] = msg_buffer[0];
                sensor_54_buffer[1] = msg_buffer[1];
                sensor_54_buffer[2] = msg_buffer[2];
            }
            else if (sender_id == train_77_id){ //Going to leave this in here, but this should never run
                if (status_77 != 'l') { //only allowed to process nessage if not locked
                    if (msg_buffer[0] == 'c') { //preparing to localize. Only one train can run
                        status_54 = 'l'; //lock 54 from request
                        status_77 = 'c';
                    }
                    else if (msg_buffer[0] == 'n') { //get next sensor / localize. Only one train can run
                        status_54 = 'l'; //lock 54 from request
                        status_77 = 'n';
                    }
                    else if (msg_buffer[0] == 'f') { //finished localization. Unlock
                        Reply(train_77_id, go_msg, 3); 
                        if (sensor_54_buffer[0] == 'c'){ //may have received an intermediate 'c' request from 77
                            status_54 = 'c'; 
                            status_77 = 'l';
                        }
                        else { //otherwise set both to ready
                            status_77 = 'r'; 
                            status_54 = 'r';
                        }
                    }
                    else if (msg_buffer[0] == 'd') { //Done with a route
                        status_77 = 'd';
                        Reply(train_77_id, go_msg, 3); 
                    }
                    else if (msg_buffer[0] == 'i') { //ignore while reversing
                        //uart_printf(CONSOLE, "sensor ignoring 77 for %d \r\n", 1000000 * msg_buffer[1] + 100000 * msg_buffer[2]);
                        wait_77_time = get_current_time() + 1000000 * msg_buffer[1] + 100000 * msg_buffer[2]; //3 seconds
                        status_77 = 'i';
                        Reply(train_77_id, go_msg, 3); 
                    }
                    else if (msg_buffer[0] == 'p') { //train is replanning, ignore until its ready
                        status_77 = 'p';
                        Reply(train_77_id, go_msg, 3);
                    }
                    else if (msg_buffer[0] == 'r') { //train is finished replanning and ready to move again
                        status_77 = 'r';
                        Reply(train_77_id, go_msg, 3);
                    }
                    else if (msg_buffer[1] == 'R') { //skip MR / BR node messages that somehow get sent
                        status_77 = 'r';
                        Reply(train_77_id, go_msg, 3);
                    }
                    else if (msg_buffer[0] == 'F') { //Fake Sensor.
                        wait_77_time = get_current_time() + 600000;//0.6 seconds
                        status_77 = 'i';
                    }
                    else {
                        status_77 = 's'; //polling for a sensor value
                        if (elapse_time_77 != 0) {
                            //uart_printf(CONSOLE, "Time Waiting for 77 request: %d ms \r\n", (get_current_time() - elapse_time_54)/1000);
                        }
                    }
                }
                else {
                   // uart_printf(CONSOLE, "Sensor query Train 77 locked\r\n");
                }
                //uart_printf(CONSOLE, "77 sent server request: %s \r\n", msg_buffer);
                sensor_77_buffer[0] = msg_buffer[0];
                sensor_77_buffer[1] = msg_buffer[1];
                sensor_77_buffer[2] = msg_buffer[2];
            }
        }

        Request_Sensor_Data(uart_server_id);
 
        sensor_data = Getc(uart_server_id, MARKLIN);
        Process_sensor_data(sensor_data, 'A', 0, triggered_sensor_buffer);

        sensor_data = Getc(uart_server_id, MARKLIN);
        Process_sensor_data(sensor_data, 'A', 1, triggered_sensor_buffer);

        sensor_data = Getc(uart_server_id, MARKLIN);
        Process_sensor_data(sensor_data, 'B', 0, triggered_sensor_buffer);

        sensor_data = Getc(uart_server_id, MARKLIN);
        Process_sensor_data(sensor_data, 'B', 1, triggered_sensor_buffer);

        sensor_data = Getc(uart_server_id, MARKLIN);
        Process_sensor_data(sensor_data, 'C', 0, triggered_sensor_buffer);

        sensor_data = Getc(uart_server_id, MARKLIN);
        Process_sensor_data(sensor_data, 'C', 1, triggered_sensor_buffer);

        sensor_data = Getc(uart_server_id, MARKLIN);
        Process_sensor_data(sensor_data, 'D', 0, triggered_sensor_buffer);

        sensor_data = Getc(uart_server_id, MARKLIN);
        Process_sensor_data(sensor_data, 'D', 1, triggered_sensor_buffer);

        sensor_data = Getc(uart_server_id, MARKLIN);
        Process_sensor_data(sensor_data, 'E', 0, triggered_sensor_buffer);

        sensor_data = Getc(uart_server_id, MARKLIN);
        Process_sensor_data(sensor_data, 'E', 1, triggered_sensor_buffer);

        if (status_54 == 's') { // 54 Finding a Specific Sensor
            if (((triggered_sensor_buffer[0][0] == sensor_54_buffer[0]) && (triggered_sensor_buffer[0][1] == sensor_54_buffer[1]) && ((triggered_sensor_buffer[0][2] == sensor_54_buffer[2])) || (triggered_sensor_buffer[1][0] == sensor_54_buffer[0]) && (triggered_sensor_buffer[1][1] == sensor_54_buffer[1]) && (triggered_sensor_buffer[1][2] == sensor_54_buffer[2]))){
                uart_printf(CONSOLE, "\033[35;1HComputer Desired Sensor %s triggered \033[37m\r\n", triggered_sensor_buffer[0]); //line 35
                desire_sensor_triggered_54 = 1;
            }
        }
        else if (status_54 == 'n'){ // 54 Localization
            if (triggered_sensor_buffer[0][0] != 'Z'){ //Only need to check one char to determine if any sensor has been triggered
                desire_sensor_triggered_54 = 1;
            }
        }

        if (status_77 == 's') { // 77 Finding a Specific Sensor
            if (((triggered_sensor_buffer[0][0] == sensor_77_buffer[0]) && (triggered_sensor_buffer[0][1] == sensor_77_buffer[1]) && (triggered_sensor_buffer[0][2] == sensor_77_buffer[2])) || ((triggered_sensor_buffer[1][0] == sensor_77_buffer[0]) && (triggered_sensor_buffer[1][1] == sensor_77_buffer[1]) && (triggered_sensor_buffer[1][2] == sensor_77_buffer[2]))){
                uart_printf(CONSOLE, "\033[36;1HUser Desired Sensor %s triggered \033[37m\r\n", triggered_sensor_buffer[0]); //line 36
                desire_sensor_triggered_77 = 1;
            }
        }
        else if (status_77 == 'n'){
            if (triggered_sensor_buffer[0][0] != 'Z'){ //Only need to check one char to determine if any sensor has been triggered
                desire_sensor_triggered_77 = 1;
            }
        }

        end = get_current_time();

        if (status_54 == 'c'){
            //uart_printf(CONSOLE, "SQ replying confirm 54\r\n");
            Reply(train_54_id, go_msg, 3); 
            status_54 = 'r';
            sensor_54_buffer[0] = 'r';
            sensor_54_buffer[1] = 'd';
            sensor_54_buffer[2] = 'y';
        }
        else if (desire_sensor_triggered_54 && status_54 == 'n'){
            //uart_printf(CONSOLE, "SQ replying next sensor 54\r\n");
            sensor_time_msg[0] = triggered_sensor_buffer[0][0];
            sensor_time_msg[1] = triggered_sensor_buffer[0][1];
            sensor_time_msg[2] = triggered_sensor_buffer[0][2];
            sensor_time_msg[3] = 0;
            Reply(train_54_id, sensor_time_msg, 10); 
            status_54 = 'r';
            sensor_54_buffer[0] = 'r';
            sensor_54_buffer[1] = 'd';
            sensor_54_buffer[2] = 'y';
        }
        else if (desire_sensor_triggered_54 && status_54 == 's'){
            *raw_sensor_time = end;
            elapse_time_54 = end;
            Reply(train_54_id, sensor_time_msg, 10); 
            status_54 = 'r';
            sensor_54_buffer[0] = 'r';
            sensor_54_buffer[1] = 'd';
            sensor_54_buffer[2] = 'y';
        }

        if (status_77 == 'c'){
            Reply(train_77_id, go_msg, 3); 
            status_77 = 'r';
            sensor_77_buffer[0] = 'r';
            sensor_77_buffer[1] = 'd';
            sensor_77_buffer[2] = 'y';
        }
        else if (desire_sensor_triggered_77 && status_77 == 'n'){
            sensor_time_msg[0] = triggered_sensor_buffer[0][0];
            sensor_time_msg[1] = triggered_sensor_buffer[0][1];
            sensor_time_msg[2] = triggered_sensor_buffer[0][2];
            sensor_time_msg[3] = 0;
            Reply(train_77_id, sensor_time_msg, 10); 
            status_77 = 'r';
            sensor_77_buffer[0] = 'r';
            sensor_77_buffer[1] = 'd';
            sensor_77_buffer[2] = 'y';
        }
        else if (desire_sensor_triggered_77 && status_77 == 's'){
            *raw_sensor_time = end;
            elapse_time_77 = end;
            Reply(train_77_id, sensor_time_msg, 10); 
            status_77 = 'r';
            sensor_77_buffer[0] = 'r';
            sensor_77_buffer[1] = 'd';
            sensor_77_buffer[2] = 'y';
        }

        loop_count++;
        
        triggered_sensor_buffer[0][0] = 'Z';
        triggered_sensor_buffer[0][1] = 'Z';
        triggered_sensor_buffer[0][2] = 'Z';
        desire_sensor_triggered_54 = 0;
        desire_sensor_triggered_77 = 0;
    }

    uart_printf(CONSOLE, "Sensor Query exiting \r\n");
    Exit();
}

void Train_Control_Task() {
    //Get servers
    char uart_server_name [14] = "Wuart_server"; //first W is to encode as WhoIs message 
    int uart_server_id;
    uart_server_id = WhoIs(uart_server_name);
    char console_server_name [20] = "Wconsole_server"; //first W is to encode as WhoIs message 
    int console_server_id;
    console_server_id = WhoIs(console_server_name);
    uart_printf(CONSOLE, "Found Console Server is %d\r\n", console_server_id);
    char clock_server_name[14] = "Wclock_server"; //first W is to encode as WhoIs message 
    int clock_server_id;
    clock_server_id = WhoIs(clock_server_name);
    // General Stuff
    int sender_id = 0;
    char server_msg_buffer[20] = {0};

    int train_input = 0;
    uint8_t train_idx = 0;
    char case_response[3] = {'z', 0, 0};//{'5', 0, 0};
    uart_printf(CONSOLE, "Select Testing Case:\r\n");
    uart_printf(CONSOLE, "[a] 1 Euler Path (54)   [b] Single User (77) [c] Full Project (54 and 77) \r\n");
    while (train_input != 'a' && train_input != 'b' && train_input != 'c'){
        train_input = Getc(uart_server_id, CONSOLE);
    }
    switch(train_input){
        case 'a':
            case_response[0] = 'a';
            uart_printf(CONSOLE, "Selected Single Euler Path Train (54)!\r\n");
            break;
        case 'b':
            case_response[0] = 'b';
            uart_printf(CONSOLE, "Selected Single Use Train (Use 77)!\r\n");
            break;
        case 'c':
            case_response[0] = 'c';
            uart_printf(CONSOLE, "Selected Full Project (Use 54 and 77)!\r\n");
            break;
    }
    
    //case responses
    char a_response[3] = {'a', 0, 0};
    char b_response[3] = {'b', 0, 0};
    char c_response[3] = {'c', 0, 0};

    //go response
    char go_response[3] = {'G', 'O', 0};

    int sensor_task_id = Create(7, Query_Sensors); //Total tasks = 10
    uart_printf(CONSOLE, "Created Query_Sensors with prio 6 at tid: %d\r\n", sensor_task_id);
    //Let the Query Sensor do its setup
    Receive(&sender_id, server_msg_buffer, 20); //Block the Sensor query until the other have name registered

    //train task ids
    int train_54_id = 0;
    int train_77_id = 0;
    //train responses
    char response_train_54[3] = {'5','4', 0};
    char response_train_77[3] = {'7', '7', 0};
    char destination_77[4] = {0, 0, 0, 0};

    if (train_input == 'a' ||train_input == 'c') {
        train_54_id = Create(8, Euler_Train_Task); //Total tasks = 10
        Receive(&sender_id, server_msg_buffer, 20); //Receive Train 54 id req
        uart_printf(CONSOLE, "Created Train 54 with prio 4 at tid: %d\r\n", train_54_id);
    }
    if (train_input == 'b' ||train_input == 'c') {
        train_77_id = Create(8, User_Train_Task); //Total tasks = 11
        Receive(&sender_id, server_msg_buffer, 20); //Receive Train 77 id req
        uart_printf(CONSOLE, "Created Train 77 with prio 4 at tid: %d\r\n", train_77_id);
    }

    //Because we don't epect any message from other tasks and don't care about the message contents, predfined order of messages, since we block them
    if (train_input == 'a' || train_input == 'c')
        Reply(train_54_id, response_train_54, 3); //Reply 54 id
    if (train_input == 'b' || train_input == 'c')
        Reply(train_77_id, response_train_77, 3); //Reply 77 id

    uart_printf(CONSOLE, "Sent ids\r\n");
    //Confirm Registered at the name server, inform of select run case. Also a synchronization tool
    if (train_input == 'a' || train_input == 'c')
        Receive(&sender_id, server_msg_buffer, 20);
    if (train_input == 'b' || train_input == 'c')
        Receive(&sender_id, server_msg_buffer, 20);
    if (train_input == 'a' || train_input == 'c')
        Reply(train_54_id, case_response, 3);
    if (train_input == 'b' || train_input == 'c')
        Reply(train_77_id, case_response, 3);
    uart_printf(CONSOLE, "confirmed train registered\r\n");

    Reply(sensor_task_id, case_response, 3); //Release sensor task

    uart_printf(CONSOLE, "confirmed all registered\r\n");

    //Synch up localization for both trains
    if (train_input == 'a' ||train_input == 'c')
        Receive(&sender_id, server_msg_buffer, 20); //Receive
    if (train_input == 'b' ||train_input == 'c')
        Receive(&sender_id, server_msg_buffer, 20); //Receive
    uart_printf(CONSOLE, "Localization Finished \r\n");

    setup_GUI(console_server_id);
    set_all_A(console_server_id, 0);
    set_all_B(console_server_id, 0);
    set_all_C(console_server_id, 0);
    set_all_D(console_server_id, 0);
    set_all_E(console_server_id, 0);

    if (train_input == 'a' ||train_input == 'c')
        Reply(train_54_id, go_response, 3);
    if (train_input == 'b' ||train_input == 'c')
        Reply(train_77_id, go_response, 3);

    //This is only for the User train atm. Polling for new destination

    Exit();
}

void FirstUserTask(){ //Total task = 2
    int returned_task_id = 0; 
    int uart_server_id = 0;
    int console_server_id = 0;
    int clock_server_id = 0;

   char switch_initial_value = '?';
    for (int i = 0; i < 18; i++){
        switch_data[i][0] = i+1;
        switch_data[i][1] = switch_initial_value;
    }
    //The switch ids skip some numbers and continue at 153
    switch_data[18][0] = (uint8_t)153;
    switch_data[18][1] = switch_initial_value;
    switch_data[19][0] = (uint8_t)154;
    switch_data[19][1] = switch_initial_value;
    switch_data[20][0] = (uint8_t)155;
    switch_data[20][1] = switch_initial_value;
    switch_data[21][0] = (uint8_t)156;
    switch_data[21][1] = switch_initial_value;

    uart_printf(CONSOLE, "First User task exists tid:\r\n"); //total tasks 2
    uart_server_id = Create(3, uart_server_task);
    uart_printf(CONSOLE, "Created Uart Server task at tid: %d\r\n", uart_server_id); //Total task = 5
    clock_server_id = Create(1, Clock_server_task); 
    uart_printf(CONSOLE, "Created Clock Server task at tid: %d\r\n", clock_server_id); //Total tasks = 7
    returned_task_id = Create(15, Idle_task); //Total tasks = 8
    uart_printf(CONSOLE, "Created Idle task at tid: %d\r\n", returned_task_id);
    
    console_server_id = Create(14, console_server_task); // total tasks: 9
    uart_printf(CONSOLE, "Created Console Server task at tid: %d\r\n", console_server_id);

    Delay(clock_server_id, 50);
    uart_printf(CONSOLE,"\033[?25l");
    setup_GUI(console_server_id);
    set_all_A(console_server_id, 0);
    set_all_B(console_server_id, 0);
    set_all_C(console_server_id, 0);
    set_all_D(console_server_id, 0);
    set_all_E(console_server_id, 0);
    set_Dummy_sensors(console_server_id, 0);
    set_Dummy_messages(console_server_id);

    int display_clock = Create(10, Display_Clock); // total tasks: 9
    //uart_printf(CONSOLE, "Created Display Clock task at tid: %d\r\n", display_clock);
    Delay(clock_server_id, 60000);

    uart_printf(CONSOLE, "Select Track:\r\n");
    uart_printf(CONSOLE, "[a] Track A     [b] Track B \r\n");
    while (track_input != 'a' && track_input != 'b'){
        track_input = Getc(uart_server_id, CONSOLE);
    }
    if (track_input == 'a'){
        init_tracka(Track_B);
    }
    else if (track_input == 'b') {
        init_trackb(Track_B);
    }
    
    returned_task_id = Create(6, Train_Control_Task); //Total tasks = 13
    uart_printf(CONSOLE, "Created Train Control Parser with prio 4 at tid: %d\r\n", returned_task_id);

    uart_printf(CONSOLE, "FirstUserTask: exiting\r\n");
    Exit();
}


int kmain(){

    Kernel_initialize(5, FirstUserTask);

    Kernel_loop();

    // Return success
    return 0;
}
