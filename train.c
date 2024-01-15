#include <stdbool.h>
#include <stdint.h>
#include "track.h"
#include "a0.h"
#include "rpi.h"
#include "interrupts.h"
#include "uart_server.h"
#include "tasks.h"
#include "clock.h"
#include "util.h"
#include "GUI.h"

#define train_replanned 54


bool checkEntry(int i){
    return (i ==  62 || i == 66 || i == 79 || i == 18); 
}
bool checkExit(int i){
    return (i ==  40 || i == 43 || i == 68 || i == 77); 
}

//Train Speed, Actual speed (mm/s), Stopping distance um = mm*1000
struct Train_info Known_Trains[NUM_CHAR_TRAIN] = { {54, {{5, 223, 280000}, {7, 413, 577000}, {13, 607, 886000}, {4, 169, 177000}, {3, 120, 120000}} }, \
                                            {77, {{5, 98, 106000}, {7, 283, 438000}, {13, 546, 1230000}, {7, 185, 260000}, {6, 185, 260000}} }};

extern struct track_node Track_B[TRACK_MAX];
extern char switch_data[NUM_SWITCHES][2];
extern int track_input;
//extern volatile bool isPrinting;
struct Train Trains[2];

int setOffset(int sensorIdx1, int sensorIdx2){
    // Set curveOffset
    if (sensorIdx1 == 40 || sensorIdx1 == 43 || sensorIdx1 == 44 || sensorIdx1 == 68 || sensorIdx1 == 77){
        if (sensorIdx2 == 19 || sensorIdx2 == 78 || sensorIdx2 == 2 || sensorIdx2 == 67 || sensorIdx2 == 63){
            return 2;
        }
    }
    return 1*(Track_B[sensorIdx1].isCurve);
}

// Return 0 if success
// Return 1 if not
//
// bookSegment returns 0 if its successful, 1 Otherwise
// bookSegment sets the node to be in use if it is successful
// bookSegment doesn't do anything if it is not successful
bool bookSection(int train_idx, next_step* sequence, int seqIdx){
    bool isBooked = false;
    if (!bookSegment(Track_B, train_idx, sequence[seqIdx].next_sensor_id)){
        for (int i = 0; i < sequence[seqIdx].switch_count; i++){
            // if it is not successful at booking a switch
            // it will have to unbook all the previously booked switches
            if (bookSegment(Track_B, train_idx, sequence[seqIdx].switchID[i])){
                isBooked = true;
                for (int j = i-1; j >= 0; j--){
                    UnbookSegment(Track_B, train_idx, sequence[seqIdx].switchID[j]);
                }
            }
            break;
        }
    }
    else {
        isBooked = true;
    }
    return isBooked;
}

bool soft_Reverse(const char* g_sensor, int trainid, int sensor_id, int uart_server_id, int clock_server_id){
    char sensor_rpl_buffer[10] = {0};
    //slow down
    Send_train_command_byte(uart_server_id, trainid, 16+3);
    //wait for sensor
    Send(sensor_id, g_sensor, 4, sensor_rpl_buffer, 10);
    //stop
    Send_train_command_byte(uart_server_id, trainid, 16);
    Delay(clock_server_id, 200);
    //reverse
    Send_train_command_byte(uart_server_id, trainid, 16 + 15);
    //start up?
    return 1;
}

bool isReverse(int idx1, int idx2){
    return (idx1 == idx2) || (Track_B[idx1].reverse->id == Track_B[idx2].id);
}

void UnbookSection(int train_idx, next_step* sequence, int seqIdx){
    UnbookSegment(Track_B, train_idx, sequence[seqIdx].next_sensor_id);
    for (int i = 0; i < sequence[seqIdx].switch_count; i++){
        UnbookSegment(Track_B, train_idx, sequence[seqIdx].switchID[i]);
    }
}
bool setSwitch(int uart_server_id, int clock_server_id, int console_server_id, int train_id, next_step* sequence, int seqIdx){
    bool isSet = false;
    Console_printf(console_server_id, "Step %d with %d switches", seqIdx, sequence[seqIdx].switch_count);
    for (int j = 0; j < sequence[seqIdx].switch_count; j++) {
        Console_printf(console_server_id, "Switch %s set to %d \r\n", sequence[seqIdx].switch_name[j], sequence[seqIdx].switch_dir[j]);
        if (sequence[seqIdx].switch_dir[j] == 0) {
            Set_straight(uart_server_id, NUM_SWITCHES, Track_B[sequence[seqIdx].switchID[j]].num);
            isSet = true;
        }
        else if (sequence[seqIdx].switch_dir[j] == 1) {
            Set_branch(uart_server_id, NUM_SWITCHES, Track_B[sequence[seqIdx].switchID[j]].num);
            isSet = true;
        }
    }
    Delay(clock_server_id, 20); //200ms delay before turning solenoid off 
    Turn_switch_off(uart_server_id);
    return isSet;
}

int EulerPlan(int console_server_id, track_node* track, int start_idx, next_step* sequence, int trainID, bool* ListOfSensors, bool* initReverse){
    
    *initReverse = false;
    // if we have track B then we can optionally mark these sensors off
    bool LoS[TRACK_MAX] = {0};


    if (track_input == 'b' || track_input == 'a'){
        for (int i = 0; i < TRACK_MAX; i++){
            if (i != 0 && i != 1 && i != 12 && i != 13 && i != 4 && i != 5 && i != 6 && i != 7 && i != 8 && i != 9 && i != 22 && i != 23 && i != 24 && i != 25 && i != 26 && i != 27 && i!= 34 && i != 35 && i < 80){
                LoS[i] = ListOfSensors[i];
            }
            else {
                ListOfSensors[i] = 1;
                LoS[i] = 1;
            }
        }
    }

    LoS[start_idx] = 1;
    LoS[track[start_idx].reverse->id] = 1;

    int round = 0;
    int step_size = 0;
    for (;;){
        bool firstStepReverse = false;
        
        int new_step_size = Dijkstra(start_idx, track, sequence, step_size, trainID, console_server_id, LoS, &firstStepReverse);
        if (firstStepReverse && round == 0){
            *initReverse = true;
            Console_printf(console_server_id, "Train %d, First step needs reversing!\n", trainID);
        }
        else if (firstStepReverse && round > 0){
            // We need reverse before proceeding to the next step
            sequence[step_size - 1].needs_reverse = true;
            firstStepReverse = false;
        }
        round ++;

        // The || round > 2 is debug, only use it for testing plz!!!
        if (new_step_size == 0){ // || round > 2){
            //printPath(sequence, step_size, trainID, console_server_id);
            Console_printf(console_server_id, "%d steps total, in %d rounds\n", step_size, round);
            break;
        }
        for (int i = step_size; i < new_step_size; i++){
            start_idx = sequence[i].next_sensor_id;
            if (start_idx < 80 && LoS[start_idx] == 0){
                LoS[sequence[i].next_sensor_id] = 1;
                LoS[track[sequence[i].next_sensor_id].reverse->id] = 1;
            }
            track[sequence[i].next_sensor_id].isVisited = 1;
            track[sequence[i].next_sensor_id].reverse->isVisited = 1;
        }
        step_size = new_step_size;
    }
    return step_size;
    
}

void Euler_Train_Task(){
    next_step sequence[TRACK_MAX] = {(next_step){1, 0, "", 0, "", {0,0,0,0,0,0}, {0,0,0,0,0,0}, {"","","", "", "", ""}, 0, 0, 0, 0}};


    //Sender stuff
    char sensor_rpl_buffer[10] = {0};
    //msgs : 'n' polling for next senor, 'd' done with a route, 'c' confirm safe to start.
    char filler_msg[4] = "msg";
    char ignore_msg[4] = {'i', 0, 0, 0};
    char confirm_msg[4] = "cfm";
    char done_msg[4] = "dne";
    char finish_localization_msg[4] = "fnl";
    char next_sensor_msg[4] = "nxt";
    char pause_sensor_msg[4] = "pse";
    char ready_sensor_msg[4] = "rdy";

    //Tracking our Error
    struct Error_Tracker Model_Error[TRACK_MAX] = {0}; //Track Max = max number of steps = max number of errors
    uint32_t *sensor_time = (uint32_t *)sensor_rpl_buffer;
    
    //Get servers
    char uart_server_name [14] = "Wuart_server"; //first W is to encode as WhoIs message 
    int uart_server_id;
    uart_server_id = WhoIs(uart_server_name);
    char clock_server_name[14] = "Wclock_server"; //first W is to encode as WhoIs message 
    int clock_server_id;
    clock_server_id = WhoIs(clock_server_name);
    char console_server_name [20] = "Wconsole_server"; //first W is to encode as WhoIs message 
    int console_server_id;
    console_server_id = WhoIs(console_server_name);
    char sensor_name[14] = "Wsensor"; //first W is to encode as WhoIs message 
    int sensor_id;
    sensor_id = WhoIs(sensor_name);
    int train_ctrl_id = MyParentTid();

    //int start = 71;
    //bool LoS[TRACK_MAX] = {0};
    //bool initReverse = 0;
    //EulerPlan(console_server_id, Track_B, start, sequence, 54, LoS, &initReverse);
    uint8_t train_idx = 0;
    char info_buffer[10] = {0};
    Send(train_ctrl_id, filler_msg, 4, info_buffer, 10); //msg sent is a dummy msg
    Train *train;
    switch(info_buffer[0]){
        case '5':
            train_idx = 0;
            char register_name54 [14] = "Rtrain_54"; //first W is to encode as WhoIs message 
            int register_status54 = RegisterAs(register_name54);
            Send(train_ctrl_id, filler_msg, 4, info_buffer, 10); //dummy to let the all know we registered
            Console_printf(console_server_id, "Train 54 Task Initialized!\r\n");
            train = &Trains[0];
            train->trainID = 54;
            train->headingID = 0;
            train->locationID = 0;
            train->reverse = 0;
            break;
        case '7':
            train_idx = 1;
            char register_name77 [14] = "Rtrain_77"; //first W is to encode as WhoIs message 
            int register_status77 = RegisterAs(register_name77);
            Send(train_ctrl_id, filler_msg, 4, info_buffer, 10); //dummy to let the all know we registered
            Console_printf(console_server_id, "Train 77 Task Initialized!\r\n");
            train = &Trains[1];
            train->trainID = 77;
            train->headingID = 0;
            train->locationID = 0;
            train->reverse = 0;
            break;
    }
    
    bool isPaused = false;
    //Turn on Train Lights so we know direction + Got to this point
    Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16);
    int iter_track = 0;

    //Controller creates the sesnor query task
    //if (Known_Trains[train_idx].id == 54)
        //Setup_Big_Loop(uart_server_id, clock_server_id, switch_data, NUM_SWITCHES); Don't need to setup the loop

    //Synchronize for localization
    //Console_printf(console_server_id, "Train %d Confirmed Ready to move \r\n", Known_Trains[train_idx].id);
    //char Destination[4] = {0};
    //Send(train_ctrl_id, filler_msg, 4, Destination, 4); //msg sent is a dummy msg
    //Console_printf(console_server_id, "Goal for Train %d is sensor is %s \r\n", Known_Trains[train_idx].id, Destination);

    // Finding Starting point
    char Start_Sensor[4] = {0};
    //Receive(&sender_id, server_msg_buffer, 10); //Receive (From sensor buffer)
    Console_printf(console_server_id, "Train %d Requesting Localize \r\n", Known_Trains[train_idx].id);
    
    Send(sensor_id, confirm_msg, 4, sensor_rpl_buffer, 10);

    Console_printf(console_server_id, "Train %d Confirmed Ready to move \r\n", Known_Trains[train_idx].id);
    if (train->trainID == 54)
        Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16+2);
    else
        Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16+3);

    Send(sensor_id, next_sensor_msg, 4, sensor_rpl_buffer, 10);
    //Console_printf_n_77(console_server_id, train->trainID, "Train %d first detection \r\n", Known_Trains[train_idx].id);
    //Double check it was actually triggered because the buffer behaves weird after pauses
    Send(sensor_id, next_sensor_msg, 4, sensor_rpl_buffer, 10);
    if (train_idx == 1) {
        Delay(clock_server_id, 50); //go forward a bit to avoid stopping on switch
    }
    Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16);
    //Console_printf_n_77(console_server_id, train->trainID, "Train %d second detection \r\n", Known_Trains[train_idx].id);

    // Reply(sender_id, pause_reply, 4); //Stop polling
    Start_Sensor[0] = sensor_rpl_buffer[0];
    Start_Sensor[1] = sensor_rpl_buffer[1];
    Start_Sensor[2] = sensor_rpl_buffer[2];

    //Console_printf_n_77(console_server_id, "%d stopped at starting point", train->trainID);
    //Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16);
    //if (train_idx == 0)
    Console_printf(console_server_id, "Train %d Detected First Sensor: %s \r\n", Known_Trains[train_idx].id, Start_Sensor);
    //Removed Some Waypoint Stuff + Loop Setup Stuff

    //int dest_idx = trackNameToIdx(Destination, Track_B);
    int start_idx = trackNameToIdx(Start_Sensor, Track_B);

    //Reverse back over sensor
    Delay(clock_server_id, 200);
    Send_train_command_byte(uart_server_id, train->trainID, 16+15);
    Delay(clock_server_id, 50);
    //int time_go = dist_go *100/ (Known_Trains[train_idx].speeds[speed_case][1] * 1000) ; //num ticks to go
    Send_train_command_byte(uart_server_id, train->trainID, 16+3);
    if (train->trainID==77)
        Delay(clock_server_id, 250);
    else   
        Delay(clock_server_id, 150);
    Send_train_command_byte(uart_server_id, train->trainID, 16);
    Delay(clock_server_id, 200);
    Send_train_command_byte(uart_server_id, train->trainID, 16+15);
    //reverse so going in the right direction again

    Send(sensor_id, finish_localization_msg, 4, sensor_rpl_buffer, 10);
    Console_printf(console_server_id, "Train %d Detected waiting for other train to Localize \r\n", Known_Trains[train_idx].id);
    Send(train_ctrl_id, filler_msg, 4, info_buffer, 10); //msg sent is a dummy msg
    Send_train_command_byte(uart_server_id, train->trainID, 16);
    Delay(clock_server_id, 200);

    uint32_t benchmarking_time = get_current_time();
    Console_printf(console_server_id, "Train %d starting @ %u \r\n", train->trainID, benchmarking_time);

    train->locationID = start_idx;

    
        
    // Start of handler, end of localization
    
    bool initialReverse = false;
    bool ListOfSensors[TRACK_MAX] = {0};

    int step_size = EulerPlan(console_server_id, Track_B, start_idx, sequence, train->trainID, ListOfSensors, &initialReverse);
    train->headingID = sequence[0].next_sensor_id;

    ListOfSensors[start_idx] = 1; //make sure first sensor is marked off
    log_sensor(console_server_id, train->trainID, start_idx);

    if (initialReverse){
        Send_train_command_byte(uart_server_id, train->trainID, 16+15);
        initialReverse = false;
    }
    //Console_printf_n_77(console_server_id, train->trainID, "%d getting started", train->trainID);

    //Train 77 uses speed 7, Train 54 uses speed 4
    uint8_t speed_case = 3; 

    uint32_t prev_time = get_current_time();

    // Forced Allocation because we have to book it either way
    // Init Train data
    train->reverse = false;
    train->locationID = start_idx;
    bookSegment(Track_B, train->trainID, start_idx);
    printBookings(Track_B, 1, console_server_id);
    
    int offset_direction = 0;
    int offset_dist = 0;

    // Train Control Logic
    if (step_size > 0){
        train->headingID = sequence[0].next_sensor_id;
    }
    else {
        train->headingID = start_idx;
    }

    bool isStopping = false;
    uint32_t last_go_at = get_current_time();
    int curveOffset = 0;

    // TODO: If the first step goes to itself, just book itself and exit


    //Console_printf_n_77(console_server_id, train->trainID, "Is deadlocked %d", Deadlocked(console_server_id));
    // Look to see if the first section (step) is booked 
    while (bookSection(train->trainID, sequence, 0)){
        if (!isPaused){
            isPaused = true;
            Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
        }
        // Wait for it to clear
        Send_train_command_byte(uart_server_id, train->trainID, 16);
        Console_printf(console_server_id, "Train %d Waiting for Section %s", train->trainID, sequence[0].next_sensor_name);
        if (Deadlocked(console_server_id)){
            printBookings(Track_B, 1, console_server_id); // Track B
            // Replan if possible
            if (train->trainID != train_replanned){
                //Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);
                step_size = EulerPlan(console_server_id, Track_B, train->locationID, sequence, train->trainID, ListOfSensors, &initialReverse);
                if (initialReverse){
                    Delay(clock_server_id, 100);
                    Send_train_command_byte(uart_server_id, train->trainID, 16+15);
                    initialReverse = false;
                }
                if (step_size == 0){
                    Console_printf(console_server_id, "This is %d, We are stuck!!!!", train->trainID);
                }
            }
        }
    }

    if (isPaused) {
        isPaused = false;
        Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
    }
    Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[0].next_sensor_name);

    // Initial Reverse is no more
    /*
    if (sequence[0].needs_reverse && isReverse(sequence[0].next_sensor_id, start_idx)){
        Console_printf(console_server_id, "Train %d Initial Reverse", train->trainID);
        //Console_printf_n_77(console_server_id, train->trainID, "Initial Reverse dist %d", sequence[0].dist_to_sensor);
        //ignore_msg[1] = 0;
        //ignore_msg[2] = 5;
        if (!isPaused){
            isPaused = true;
            Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
        }
        Delay(clock_server_id, 10);
        Send_train_command_byte(uart_server_id, train->trainID, 15);
        Delay(clock_server_id, 10);
        //Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
        last_go_at = get_current_time();

        if (step_size > 1){
            train->locationID = sequence[0].next_sensor_id;
            train->headingID = sequence[1].next_sensor_id;
        }
        while (step_size > 1 && bookSection(train->trainID, sequence, 1)){
        if (!isPaused){
            isPaused = true;
            Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
        }
            // Wait for it to clear
            Send_train_command_byte(uart_server_id, train->trainID, 16);
            Console_printf(console_server_id, "Train %d Waiting for Section %s", train->trainID, sequence[1].next_sensor_name);
            if (Deadlocked(console_server_id)){
                printBookings(Track_B, 1, console_server_id); // Track B
                // Replan if possible
                if (train_replanned != train->trainID){
                    //UnbookSection(train->trainID, sequence, 1);
                    Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);
                    step_size = Dijkstra(sequence[0].next_sensor_id, dest_idx, Track_B, sequence, train->trainID, console_server_id);
                    if (step_size == 0){
                        Console_printf(console_server_id, "This is %d, We are stuck!!!!", train->trainID);
                    }
                }
                // Assume (safe to do so) that step 0 is not occupied
                bookSection(train->trainID, sequence, 0);
            }
        }
        Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[1].next_sensor_name);
    }
    */
    if (sequence[0].needs_reverse || sequence[0].hard_reverse){
        isStopping = true;
    }

    // Check switches
    if (sequence[0].switch_count > 0){
        setSwitch(uart_server_id, clock_server_id, console_server_id, train->trainID, sequence, 0);
        //Console_printf_n_77(console_server_id, train->trainID, "Train %d Switching %s", train->trainID, sequence[0].next_sensor_name);
    }

    if (isPaused) {
        isPaused = false;
        Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
    }
    Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
    last_go_at = get_current_time();
    uint32_t my_cur_time;

    //UnbookSegment(Track_B, train->trainID, start_idx);

    bool earlyStop = false;
    bool skipSensor = false;
    bool exitEmergencyStop = false;
    // If it's only one step does it unbook the last section? Does it need to?

    for (int i = 0; i < step_size - 1; i++) {
        /*
        if (first_step_rev){
            i = 1;
            first_step_rev = false;
            bookSegment(Track_B, train->trainID, sequence[0].next_sensor_id);
        }
        */
        // Try booking one step ahead
        train->locationID = sequence[i].next_sensor_id;
        train->headingID = sequence[i+1].next_sensor_id;
        
        int count = 0;
        while (bookSection(train->trainID, sequence, i+1)){
            exitEmergencyStop = true;
            // Wait for it to clear
            if (!isPaused){
                isPaused = true;
                Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
            }
            Send_train_command_byte(uart_server_id, train->trainID, 16);
            Console_printf(console_server_id, "Train %d, emergency stop! \r\n", train->trainID);
            printBookings(Track_B, 1, console_server_id); // Track B
            Delay(clock_server_id, 100);
            count ++;
            Console_printf(console_server_id, "Train %d booking: %s", train->trainID, sequence[i+1].next_sensor_name);
            if (Deadlocked(console_server_id)){
                // Replan if possible
                // Replan without booked edges
                printBookings(Track_B, 1, console_server_id); // Track B
                Console_printf(console_server_id, "Train %d Deadlocked", train->trainID);

                // Copy over last step in preparation for releasing
                next_step lastStep[1];
                lastStep[0].next_sensor_id = sequence[i].next_sensor_id;
                lastStep[0].switch_count = sequence[i].switch_count;
                for (int j = 0; j < lastStep[0].switch_count; j++){
                    lastStep[0].switchID[j] = sequence[i].switchID[j];
                    lastStep[0].switch_dir[j] = sequence[i].switch_dir[j];
                    lastStep[0].switch_name[j] = sequence[i].switch_name[j];
                }
                if (train_replanned != train->trainID){
                    //Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);
                    int prior_step_idx = 0;
                    if (i == 0){
                        prior_step_idx = start_idx;
                    }
                    else {
                        prior_step_idx = sequence[i-1].next_sensor_id;
                    }
                    step_size = EulerPlan(console_server_id, Track_B, prior_step_idx, sequence, train->trainID, ListOfSensors, &initialReverse);
                    if (initialReverse){
                        Delay(clock_server_id, 100);
                        Send_train_command_byte(uart_server_id, train->trainID, 16+15);
                        initialReverse = false;
                    }

                    if (step_size == 0 || step_size == -1){
                        Console_printf(console_server_id,"This is %d, We are stuck!!!!\n", train->trainID);
                    }
                    else {
                        // Startup Sequence
                        i = 0;
                        // Look to see if the first section (step) is booked 
                        while (bookSection(train->trainID, sequence, 0)){
                            if (Deadlocked(console_server_id)){
                                // Replan if possible
                                //Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);
                                int prior_step_idx = 0;
                                if (i == 0){
                                    prior_step_idx = start_idx;
                                }
                                else {
                                    prior_step_idx = sequence[i-1].next_sensor_id;
                                }
                                step_size = EulerPlan(console_server_id, Track_B, prior_step_idx, sequence, train->trainID, ListOfSensors, &initialReverse);
                                if (initialReverse){
                                    Delay(clock_server_id, 100);
                                    Send_train_command_byte(uart_server_id, train->trainID, 16+15);
                                    initialReverse = false;
                                }
                                if (step_size == 0){
                                    Console_printf(console_server_id, "This is %d, We are stuck!!!!", train->trainID);
                                }
                            }
                            // Wait for it to clear
                            Send_train_command_byte(uart_server_id, train->trainID, 16);
                        }
                        //Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[0].next_sensor_name);
                        if (isPaused) {
                            isPaused = false;
                            Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                        }

                        if (sequence[0].switch_count > 0){
                            setSwitch(uart_server_id, clock_server_id, console_server_id, train->trainID, sequence, 0);
                            Console_printf_n_77(console_server_id, train->trainID, "Train %d Switching %s", train->trainID, sequence[0].next_sensor_name);
                        }
                        /*
                        // Fix the first step reverse
                        // seems like first step reverse just reverses on the stop even if it wasnt supposed to be on the spot
                        if (sequence[0].needs_reverse && isReverse(sequence[0].next_sensor_id, start_idx)){
                            //Console_printf_n_77(console_server_id, train->trainID, "Train %d Initial Reverse", train->trainID);
                            //ignore_msg[1] = 1;
                            //ignore_msg[2] = 0;
                            Delay(clock_server_id, 100);
                            last_go_at = get_current_time();
                            Send_train_command_byte(uart_server_id, train->trainID, 15);
                            i = 1; // ignore first step
                        }
                        */ 
                        /*
                        else if (sequence[0].needs_reverse || sequence[0].hard_reverse){
                            // We always end up in this case anyways
                            // So we need to fix this
                            UnbookSection(train->trainID, lastStep, 0);
                            Console_printf(console_server_id, "This should not be the case\r\n");
                            Delay(clock_server_id, 100);
                            Send_train_command_byte(uart_server_id, train->trainID, 15);
                            last_go_at = get_current_time();
                            i = 1; // ignore first step
                            //Console_printf(console_server_id, "This should not be the case\r\n");
                            //isStopping = true;
                        }
                        else {
                            Console_printf(console_server_id, "This should not be the case 2\r\n");
                            UnbookSection(train->trainID, lastStep, 0);
                            Console_printf(console_server_id, "This should not be the case\r\n");
                            Delay(clock_server_id, 100);
                            Send_train_command_byte(uart_server_id, train->trainID, 15);
                            last_go_at = get_current_time();
                            i = 1; // ignore first step
                            //Console_printf_n_77(console_server_id, train->trainID, "Train %d go straight initial", train->trainID);
                            last_go_at = get_current_time();
                            // Need to deal with the case of 1 step  
                        }
                        */

                        if (sequence[0].needs_reverse || sequence[0].hard_reverse){
                            isStopping = true;
                        }
                        //UnbookSegment(Track_B, train->trainID, lastStep[0].next_sensor_id);
                    }
                }
            }
        } //End of booksection
        Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[i+1].next_sensor_name);
        printBookings(Track_B, 1, console_server_id); // Track B

        if (exitEmergencyStop){
            Console_printf(console_server_id, "Exiting emergenecy stop \r\n");
            // Redo startup
            if (isPaused) {
                isPaused = false;
                Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
            }
            Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
            last_go_at = get_current_time();
            exitEmergencyStop = false;
        }

        // Set switches
        if (i < step_size - 1) {//check if switches to trip
            if (setSwitch(uart_server_id, clock_server_id, console_server_id, train->trainID, sequence, i+1))
                ;
            //Console_printf_n_77(console_server_id, train->trainID, "Train %d Switching %s", train->trainID, sequence[i+1].next_sensor_name);
        }

        // Check Reverses
        if (isStopping){
            //Console_printf_n_77(console_server_id, train->trainID, "Last go sent %d\r\n", get_current_time() - last_go_at);
            int curveOffset = 1000 * setOffset(sequence[i].next_sensor_id, sequence[i].next_sensor_id);
            int dist_go = sequence[i].dist_to_sensor * 1000;
            Console_printf(console_server_id, "Train 54 db isStopping\r\n");
            dist_go -= Known_Trains[train_idx].speeds[speed_case][2];
            Console_printf_n_77(console_server_id, train->trainID, "Dist go 2 is %d\r\n", dist_go);
            dist_go -= curveOffset;
            Console_printf_n_77(console_server_id, train->trainID, "Dist go 3 is %d\r\n", dist_go);
            
            if (train->trainID == 54)
                dist_go += 1000 * 15; // Hardcode a little offset to make sure it trips
            else
                dist_go += 1000 * 35;
            Console_printf_n_77(console_server_id, train->trainID, "Dist go 4 is %d\r\n", dist_go);
            
            int time_go = 0;
            // time should enough
            if (dist_go > 0){
                int time_go = dist_go *100/ (Known_Trains[train_idx].speeds[speed_case][1] * 1000) ; //num ticks to go
                my_cur_time = get_current_time();
                Console_printf(console_server_id, "Ea Last go was %u \r\n", my_cur_time - last_go_at);
                if ((uint32_t)(my_cur_time - last_go_at) < (uint32_t)(500000)){//add an offset if only start moving recently (short moves)
                    //if (i > 0 && setOffset(sequence[i-1].next_sensor_id, sequence[i-1].next_sensor_id))
                    int extra_delay = 1 + ((my_cur_time - last_go_at - 1) / 100000);
                    time_go += 400 - extra_delay * 27 ;//+ extra_delay*2;
                    Console_printf(console_server_id, "Adding Last go Offset \r\n");
                    //time_go += 165;
                }
                if (!isPaused){
                    isPaused = true;
                    Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                }
                Console_printf(console_server_id, "Case 0, Delay for %d \r\n", time_go);
                Delay(clock_server_id, time_go);
                Send_train_command_byte(uart_server_id, train->trainID, 16);

                int stopping_time = 400; // 3 seconds stopping time
                //ignore_msg[1] = 4;
                //ignore_msg[2] = 3;
                if (!isPaused){
                    isPaused = true;
                    Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                }
                ListOfSensors[sequence[i].next_sensor_id] = 1;
                Delay(clock_server_id, stopping_time);

                //Update the UI
                log_sensor(console_server_id, train->trainID, sequence[i].next_sensor_id); 

                if (isPaused) {
                    isPaused = false;
                    Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                }

                // need to check soft_reverse
                // because it seems like it's still checking for it
                if (sequence[i].needs_reverse || sequence[i].hard_reverse){
                    Send_train_command_byte(uart_server_id, train->trainID, 15);
                    Delay(clock_server_id, 10);
                }
                
                Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
                last_go_at = get_current_time();
            }
            else { 
                Console_printf(console_server_id, "You are in a wrong case! Delay %d\r\n", time_go);
                Send_train_command_byte(uart_server_id, train->trainID, 16);
            }
            isStopping = false; 
            //skipSensor = true; //skip the sensor for this iteration since we should have already stopped on it
        }

        // Send the train forward & sensor detection
        uart_printf(CONSOLE, "\033[32m Train %d going to: %s \033[37m\r\n", train->trainID, sequence[i].next_sensor_name);

        // Stopping Sequence
        if (sequence[i+1].needs_reverse || sequence[i+1].hard_reverse || i == step_size - 2){
            Console_printf(console_server_id, "Train 54 db Early Stop\r\n");
            isStopping = true;
            int curveOffset = 1000 * setOffset(sequence[i+1].next_sensor_id, sequence[i].next_sensor_id);
            int dist_go = sequence[i+1].dist_to_sensor * 1000;
            if (dist_go < Known_Trains[train_idx].speeds[speed_case][2]){
                // Set visited
                ListOfSensors[sequence[i].next_sensor_id] = 1;
                log_sensor(console_server_id, train->trainID, sequence[i].next_sensor_id);
                

                dist_go += 1000*sequence[i].dist_to_sensor; 
                dist_go -= Known_Trains[train_idx].speeds[speed_case][2];
                dist_go -= curveOffset;
                if (train->trainID == 77){
                    dist_go += 1000*60;
                }
                int time_go = 0;
                // Calculate based on stopping distance
                // if time is enough
                if (dist_go > 0){
                    time_go = dist_go *100/ (Known_Trains[train_idx].speeds[speed_case][1] * 1000) ; //num ticks to go
                    Console_printf(console_server_id, "Case 1, Delay for %d \r\n", time_go);
                    if (!isPaused){
                        isPaused = true;
                        Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                    }
                    my_cur_time = get_current_time();
                    Console_printf(console_server_id, "Sc Last go was %u \r\n", my_cur_time - last_go_at);
                    if ((my_cur_time - last_go_at) < 500000){//add an offset if only start moving recently (short moves)
                        //if (i > 0 && setOffset(sequence[i-1].next_sensor_id, sequence[i-1].next_sensor_id))
                        int extra_delay = 1 + ((my_cur_time - last_go_at - 1) / 100000);
                        time_go += extra_delay*40 ;//+ extra_delay*2;
                        Console_printf(console_server_id, "Adding Last go Offset \r\n");
                    }
                    
                    Delay(clock_server_id, time_go);
                    Send_train_command_byte(uart_server_id, train->trainID, 16);
                }
                // stop immediately if time is not enough
                else{ 
                    Console_printf(console_server_id, "Emergency Stop! \r\n");
                    if (!isPaused){
                        isPaused = true;
                        Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                    }
                    Send_train_command_byte(uart_server_id, train->trainID, 16);
                }
                // Since this handles two steps, we skip the first sensor
                Console_printf(console_server_id, "Train %d, Unbook section %s\n\r", train->trainID, sequence[i].next_sensor_name);
                UnbookSection(train->trainID, sequence, i-1); 
                i++;
                ListOfSensors[sequence[i].next_sensor_id] = 1; //maybe not
                log_sensor(console_server_id, train->trainID, sequence[i].next_sensor_id);
                
                if (i + 1 < step_size){
                    train->headingID = sequence[i+1].next_sensor_id;

                    int stopping_time = 400; // 3 seconds stopping time
                    //ignore_msg[1] = 4;
                    //ignore_msg[2] = 0;
                    //Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                    
                    Delay(clock_server_id, stopping_time);
                    if (sequence[i].needs_reverse || sequence[i].hard_reverse){
                        Send_train_command_byte(uart_server_id, train->trainID, 15);
                    }

                    // This whole thing is a reverse
                    //
                    //
                    //
                    while (bookSection(train->trainID, sequence, i+1)){
                        exitEmergencyStop = true;
                        // Wait for it to clear
                        if (!isPaused){
                            isPaused = true;
                            Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                        }
                        Send_train_command_byte(uart_server_id, train->trainID, 16);
                        Console_printf(console_server_id, "Train %d, emergency stop! \r\n", train->trainID);
                        printBookings(Track_B, 1, console_server_id); // Track B
                        Delay(clock_server_id, 100);
                        count ++;
                        Console_printf(console_server_id, "Train %d booking: %s", train->trainID, sequence[i+1].next_sensor_name);
                        if (Deadlocked(console_server_id)){
                            // Replan if possible
                            // Replan without booked edges
                            printBookings(Track_B, 1, console_server_id); // Track B
                            Console_printf(console_server_id, "Train %d Deadlocked, replanning", train->trainID);

                            // Copy over last step in preparation for releasing
                            next_step lastStep[1];
                            lastStep[0].next_sensor_id = sequence[i].next_sensor_id;
                            lastStep[0].switch_count = sequence[i].switch_count;
                            for (int j = 0; j < lastStep[0].switch_count; j++){
                                lastStep[0].switchID[j] = sequence[i].switchID[j];
                                lastStep[0].switch_dir[j] = sequence[i].switch_dir[j];
                                lastStep[0].switch_name[j] = sequence[i].switch_name[j];
                            }
                            if (train_replanned != train->trainID){
                                //Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);

                                int prior_step_idx = 0;
                                if (i == 0){
                                    prior_step_idx = start_idx;
                                }
                                else {
                                    prior_step_idx = sequence[i-1].next_sensor_id;
                                }
                                step_size = EulerPlan(console_server_id, Track_B, prior_step_idx, sequence, train->trainID, ListOfSensors, &initialReverse);
                                if (initialReverse){
                                    Delay(clock_server_id, 100);
                                    Send_train_command_byte(uart_server_id, train->trainID, 16+15);
                                    initialReverse = false;
                                }

                                
                                if (step_size == 0 || step_size == -1){
                                    Console_printf(console_server_id,"This is %d, We are stuck!!!!\n", train->trainID);
                                }
                                else {
                                    // Startup Sequence
                                    i = 0;
                                    // Look to see if the first section (step) is booked 
                                    while (bookSection(train->trainID, sequence, 0)){
                                        if (Deadlocked(console_server_id)){
                                            // Replan if possible
                                            //Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);
                                            step_size = EulerPlan(console_server_id, Track_B, prior_step_idx, sequence, train->trainID, ListOfSensors, &initialReverse);
                                            if (initialReverse){
                                                Delay(clock_server_id, 100);
                                                Send_train_command_byte(uart_server_id, train->trainID, 16+15);
                                                initialReverse = false;
                                            }
                                            if (step_size == 0){
                                                Console_printf(console_server_id, "This is %d, We are stuck!!!!", train->trainID);
                                            }
                                        }
                                        // Wait for it to clear
                                        Send_train_command_byte(uart_server_id, train->trainID, 16);
                                    }
                                    //Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[0].next_sensor_name);
                                    if (isPaused) {
                                        isPaused = false;
                                        Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                                    }

                                    if (initialReverse){
                                        Delay(clock_server_id, 100);
                                        Send_train_command_byte(uart_server_id, train->trainID, 16+15);
                                        initialReverse = false;
                                    }
                                    /*
                                    // Fix the first step reverse
                                    // seems like first step reverse just reverses on the stop even if it wasnt supposed to be on the spot
                                    if (sequence[0].needs_reverse && isReverse(sequence[0].next_sensor_id, start_idx)){
                                        //Console_printf_n_77(console_server_id, train->trainID, "Train %d Initial Reverse", train->trainID);
                                        //ignore_msg[1] = 1;
                                        //ignore_msg[2] = 0;
                                        Delay(clock_server_id, 100);
                                        Send_train_command_byte(uart_server_id, train->trainID, 15);
                                        last_go_at = get_current_time();
                                        i = 1; // ignore first step
                                    }
                                    else if (sequence[0].needs_reverse || sequence[0].hard_reverse){
                                        UnbookSection(train->trainID, lastStep, 0);
                                        Console_printf(console_server_id, "This should not be the case\r\n");
                                        Delay(clock_server_id, 100);
                                        Send_train_command_byte(uart_server_id, train->trainID, 15);
                                        last_go_at = get_current_time();
                                        i = 1; // ignore first step
                                        //isStopping = true;
                                    }
                                    else {
                                        Console_printf(console_server_id, "This should not be the case 2\r\n");
                                        UnbookSection(train->trainID, lastStep, 0);
                                        Delay(clock_server_id, 100);
                                        Send_train_command_byte(uart_server_id, train->trainID, 15);
                                        last_go_at = get_current_time();
                                        i = 1; // ignore first step
                                        //Console_printf_n_77(console_server_id, train->trainID, "Train %d go straight initial", train->trainID);
                                        last_go_at = get_current_time();
                                        // Need to deal with the case of 1 step
                                        
                                    }
                                    */
                                    if (sequence[0].needs_reverse || sequence[0].hard_reverse){
                                        isStopping = true;
                                    }

                                    // Check switches
                                    if (sequence[i].switch_count > 0){
                                        setSwitch(uart_server_id, clock_server_id, console_server_id, train->trainID, sequence, 0);
                                        Console_printf_n_77(console_server_id, train->trainID, "Train %d Switching %s", train->trainID, sequence[0].next_sensor_name);
                                    }
                                    //UnbookSegment(Track_B, train->trainID, lastStep[0].next_sensor_id);
                                }
                            }
                        }
                    } //End of booksection
                    Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[i+1].next_sensor_name);
                    printBookings(Track_B, 1, console_server_id); // Track B

                    if (exitEmergencyStop){
                        Console_printf(console_server_id, "Exiting emergenecy stop \r\n");
                        // Redo startup
                        exitEmergencyStop = false;
                    }

                    if (sequence[i+1].switch_count > 0){
                        setSwitch(uart_server_id, clock_server_id, console_server_id, train->trainID, sequence, 1);
                        Console_printf_n_77(console_server_id, train->trainID, "Train %d Switching %s", train->trainID, sequence[1].next_sensor_name);
                    }

                    if (isPaused) {
                        isPaused = false;
                        Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                    }
                    Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
                    last_go_at = get_current_time();
                }
                else{
                    train->headingID = sequence[i].next_sensor_id;
                    earlyStop = true;
                }
                if (isPaused) {
                    isPaused = false;
                    Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                }
                isStopping = false;
            }
            else if (!sequence[i].needs_reverse && !sequence[i].hard_reverse ){ // if we reversed this iter, we already should have arrived at the sensor
                        
                Console_printf(console_server_id, "Train 54 db reverese sensor\r\n");                                                        //
                // Set visited
                ListOfSensors[sequence[i].next_sensor_id] = 1;
                log_sensor(console_server_id, train->trainID, sequence[i].next_sensor_id);


                if (checkEntry(sequence[i].next_sensor_id)){ 
                    speed_case = 3;
                    Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
                }
                else if (checkExit(sequence[i].next_sensor_id)){ 
                    speed_case = 3;
                    Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
                }
                // Poll the sensor
                //Console_printf_n_77(console_server_id, train->trainID, "Going to a sensor \r\n");
                Send(sensor_id, sequence[i].next_sensor_name, 4, sensor_rpl_buffer, 10);
            }
        }
        else if (!sequence[i].needs_reverse && !sequence[i].hard_reverse ){ // if we reversed this iter, we already should have arrived at the sensor
            Console_printf(console_server_id, "Train 54 db normal sensor\r\n");
            /* !isStopping &&  Don't think this is needed becuase is stopping is only set in the other if
            if (train->trainID == 54)
                Send_train_command_byte(uart_server_id, train->trainID, 16+4);
            else
                Send_train_command_byte(uart_server_id, train->trainID, 16+5);
            */
            // Set visited
            ListOfSensors[sequence[i].next_sensor_id] = 1;
            log_sensor(console_server_id, train->trainID, sequence[i].next_sensor_id);

            if (checkEntry(i)){ 
                speed_case = 3;
                Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
            }
            else if (checkExit(i)){ 
                speed_case = 3;
                Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
            }

            // Poll the sensor 
            Send(sensor_id, sequence[i].next_sensor_name, 4, sensor_rpl_buffer, 10);
        }
        //skipSensor = false; //skip sensor will be decided in the next iter

        // Should have reached the dest on step i by now

        // Unbook the last section
        // If we are not hard reversing on the last section, just unbook it
        // If its only two steps it might not release the first one
        if (i > 0){ // && !sequence[i].hard_reverse && !sequence[i].needs_reverse){
            //Receive(&sender_id, server_msg_buffer, 10); //Receive
            //train->locationID = sequence[i].next_sensor_id;
            //Console_printf(console_server_id, "Train %d, Unbook section %s\n\r", train->trainID, sequence[i].next_sensor_name);
            UnbookSection(train->trainID, sequence, i-1); 
            //printBookings(Track_B, 1, console_server_id); // Track B
        }
        else if (i == 0){
            UnbookSegment(Track_B, train->trainID, start_idx);
        }
        if (train->trainID == 77)
            printBookings(Track_B, 1, console_server_id);
        // Otherwise don't

        /*
        else if (i >= step_size - 4){
            int dist_till_stop = 0;
            for (int j = i; j < step_size; j++){
                dist_till_stop += sequence[j].dist_to_sensor;
            }
            if (dist_till_stop < 1600 || i == step_size - 2){
                // Slow down the train to 5 speed
                speed_case = 0;
                Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16+5);
            }
        }
        */

/*
        if (i > 0) {
            Model_Error[i-1].actual_time = *sensor_time - prev_time;
            Console_printf_n_77(console_server_id, train->trainID, "Actual time: %d \r\n", Model_Error[i-1].actual_time);
            Console_printf_n_77(console_server_id, train->trainID, "Predict time: %d \r\n", Model_Error[i-1].predict_time);
            Console_printf_n_77(console_server_id, train->trainID, "Distance was: %d \r\n", sequence[i-1].dist_to_sensor);
            Model_Error[i-1].time_error = ((int64_t) Model_Error[i-1].actual_time - (int64_t) Model_Error[i-1].predict_time); //ns
            Model_Error[i-1].dist_error = Model_Error[i-1].time_error *  (int64_t) Known_Trains[train_idx].speeds[speed_case][1] / 1000; //ns * mm / s
            Console_printf_n_77(console_server_id, train->trainID, "Error for %s: Time (us): %d | Distance (nm): %d \r\n", sequence[i-1].next_sensor_name, Model_Error[i-1].time_error/1000, Model_Error[i-1].dist_error);
        }
        prev_time = *sensor_time;
        Model_Error[i].predict_time = ((uint64_t)sequence[i].dist_to_sensor * 1000000000 / (uint64_t)Known_Trains[train_idx].speeds[speed_case][2]); //  = nm * 1000 / (mm/s) = us = our clock rate
*/
    }
    if (step_size == 1){
        UnbookSegment(Track_B, train->trainID, start_idx);
    }

    Send(sensor_id, done_msg, 4, sensor_rpl_buffer, 10);
    // This is the last step if not earlystop
    if (!earlyStop){
        int cOffset = 0;
        // Set visited
        ListOfSensors[sequence[step_size - 1].next_sensor_id] = 1;
        log_sensor(console_server_id, train->trainID, sequence[step_size - 1].next_sensor_id);
        if (step_size > 1){
            cOffset = 1000 * setOffset(sequence[step_size - 1].next_sensor_id, sequence[step_size - 2].next_sensor_id);
            UnbookSection(train->trainID, sequence, step_size - 2); 
            printBookings(Track_B, 1, console_server_id);
        }
        else 
            cOffset = 1000 * setOffset(sequence[step_size - 1].next_sensor_id, sequence[step_size - 1].next_sensor_id);
        int dist_go = sequence[step_size - 1].dist_to_sensor * 1000;
        my_cur_time = get_current_time();
        Console_printf(console_server_id, "End Last go was %u \r\n", my_cur_time - last_go_at);
        if ((uint32_t)(my_cur_time - last_go_at) < (uint32_t)(500000)){//add an offset if only start moving recently (short moves)
            //if (i > 0 && setOffset(sequence[i-1].next_sensor_id, sequence[i-1].next_sensor_id))
            //int extra_delay = 1 + ((my_cur_time - last_go_at - 1) / 100000);
            //time_go += extra_delay*40 + extra_delay*2;
            //Console_printf(console_server_id, "Adding Last go Offset \r\n");
            //time_go += 165;
            dist_go = dist_go*2;
        }
        //Console_printf_n_77(console_server_id, train->trainID, "Step 1, dist_go %d \r\n", dist_go);
        dist_go -= Known_Trains[train_idx].speeds[speed_case][2];
        //Console_printf_n_77(console_server_id, train->trainID, "Step 2, dist_go %d \r\n", dist_go);
        if (train->trainID == 54)
            dist_go -= cOffset;
        //Console_printf_n_77(console_server_id, train->trainID, "Step 3, dist_go %d \r\n", dist_go);
        dist_go += 1000 * 10; // Hardcode a little offset to make sure it trips
        //Console_printf_n_77(console_server_id, train->trainID, "Step 4, dist_go %d \r\n", dist_go);
        int time_go = dist_go *100/ (Known_Trains[train_idx].speeds[speed_case][1] * 1000) ; //num ticks to go
        /*    
        if ((my_cur_time - last_go_at) < 1000000){//add an offset if only start moving recently (short moves)
            //if (i > 0 && setOffset(sequence[i-1].next_sensor_id, sequence[i-1].next_sensor_id))
            int extra_delay = 1 + ((my_cur_time - last_go_at - 1) / 100000);
            time_go += extra_delay*40;// + extra_delay*2;
            Console_printf(console_server_id, "Adding Last go Offset \r\n");
            time_go += 165;
            //dist_go = dist_go*2;
        }             
        */
        // time should enough
        if (dist_go > 0){
            //Console_printf_n_77(console_server_id, train->trainID, "Case 0, Delay for %d \r\n", time_go);
            Delay(clock_server_id, time_go);
            Send_train_command_byte(uart_server_id, train->trainID, 16);
        }
        else { 
            //Console_printf_n_77(console_server_id, train->trainID, "Case 1, Immediate Stop! Should not happen. Delay %d\r\n", time_go);
            Send_train_command_byte(uart_server_id, train->trainID, 16);
        }
        int stopping_time = 300; // 3 second stopping time 
        Delay(clock_server_id, stopping_time);
    }
    for (int i = 0; i < sequence[step_size - 1].switch_count; i++){
        UnbookSegment(Track_B, train->trainID, sequence[step_size-1].switchID[i]);
    }
    printBookings(Track_B, 1, console_server_id);

    Console_printf(console_server_id, "Finished Train %d iteration \r\n", Known_Trains[train_idx].id);
    //int temp_idx = start_idx;
    //start_idx = dest_idx;
    //dest_idx = temp_idx;
    //Delay(clock_server_id, 200);
    //Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 15);
    Send(train_ctrl_id, filler_msg, 4, done_msg, 4); //msg sent is a dummy msg
    //Exit();

    //Send(train_ctrl_id, filler_msg, 4, Destination, 4); //msg sent is a dummy msg
    Exit();

}

void User_Train_Task(){
    next_step sequence[TRACK_MAX] = {(next_step){1, 0, "", 0, "", {0,0,0,0,0}, {0,0,0,0,0,0}, {"","","", "", "", ""}, 0, 0, 0, 0}};

    //Sender stuff
    char sensor_rpl_buffer[10] = {0};
    //msgs : 'n' polling for next senor, 'd' done with a route, 'c' confirm safe to start.
    char filler_msg[4] = "msg";
    char ignore_msg[4] = {'i', 0, 0, 0};
    char confirm_msg[4] = "cfm";
    char done_msg[4] = "dne";
    char finish_localization_msg[4] = "fnl";
    char next_sensor_msg[4] = "nxt";
    char pause_sensor_msg[4] = "pse";
    char ready_sensor_msg[4] = "rdy";

    //Tracking our Error
    struct Error_Tracker Model_Error[TRACK_MAX] = {0}; //Track Max = max number of steps = max number of errors
    uint32_t *sensor_time = (uint32_t *)sensor_rpl_buffer;
    
    //Get servers
    char uart_server_name [14] = "Wuart_server"; //first W is to encode as WhoIs message 
    int uart_server_id;
    uart_server_id = WhoIs(uart_server_name);
    char clock_server_name[14] = "Wclock_server"; //first W is to encode as WhoIs message 
    int clock_server_id;
    clock_server_id = WhoIs(clock_server_name);
    char console_server_name [20] = "Wconsole_server"; //first W is to encode as WhoIs message 
    int console_server_id;
    console_server_id = WhoIs(console_server_name);
    char sensor_name[14] = "Wsensor"; //first W is to encode as WhoIs message 
    int sensor_id;
    sensor_id = WhoIs(sensor_name);
    int train_ctrl_id = MyParentTid();

    uint8_t train_idx = 0;
    char info_buffer[10] = {0};
    Send(train_ctrl_id, filler_msg, 4, info_buffer, 10); //msg sent is a dummy msg
    Train *train;
    switch(info_buffer[0]){
        case '5':
            train_idx = 0;
            char register_name54 [14] = "Rtrain_54"; //first W is to encode as WhoIs message 
            int register_status54 = RegisterAs(register_name54);
            Send(train_ctrl_id, filler_msg, 4, info_buffer, 10); //dummy to let the all know we registered
            Console_printf(console_server_id, "Train 54 Task Initialized!\r\n");
            train = &Trains[0];
            train->trainID = 54;
            train->headingID = 0;
            train->locationID = 0;
            train->reverse = 0;
            break;
        case '7':
            train_idx = 1;
            char register_name77 [14] = "Rtrain_77"; //first W is to encode as WhoIs message 
            int register_status77 = RegisterAs(register_name77);
            Send(train_ctrl_id, filler_msg, 4, info_buffer, 10); //dummy to let the all know we registered
            Console_printf(console_server_id, "Train 77 Task Initialized!\r\n");
            train = &Trains[1];
            train->trainID = 77;
            train->headingID = 0;
            train->locationID = 0;
            train->reverse = 0;
            break;
    }
    
    bool isPaused = false;
    //Turn on Train Lights so we know direction + Got to this point
    Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16);
    int iter_track = 0;

    //Controller creates the sesnor query task
    //if (Known_Trains[train_idx].id == 54)
        //Setup_Big_Loop(uart_server_id, clock_server_id, switch_data, NUM_SWITCHES); Don't need to setup the loop

    // Finding Starting point
    char Start_Sensor[4] = {0};
    //Receive(&sender_id, server_msg_buffer, 10); //Receive (From sensor buffer)
    
   Console_printf(console_server_id, "Train %d Requesting Localize \r\n", Known_Trains[train_idx].id);
    
    Send(sensor_id, confirm_msg, 4, sensor_rpl_buffer, 10);

    Console_printf(console_server_id, "Train %d Confirmed Ready to move \r\n", Known_Trains[train_idx].id);
    if (train->trainID == 54)
        Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16+2);
    else
        Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16+3);

    Send(sensor_id, next_sensor_msg, 4, sensor_rpl_buffer, 10);
    //Console_printf_n_77(console_server_id, train->trainID, "Train %d first detection \r\n", Known_Trains[train_idx].id);
    //Double check it was actually triggered because the buffer behaves weird after pauses
    Send(sensor_id, next_sensor_msg, 4, sensor_rpl_buffer, 10);
    if (train_idx == 1) {
        Delay(clock_server_id, 50); //go forward a bit to avoid stopping on switch
    }
    Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16);
    //Console_printf_n_77(console_server_id, train->trainID, "Train %d second detection \r\n", Known_Trains[train_idx].id);

    // Reply(sender_id, pause_reply, 4); //Stop polling
    Start_Sensor[0] = sensor_rpl_buffer[0];
    Start_Sensor[1] = sensor_rpl_buffer[1];
    Start_Sensor[2] = sensor_rpl_buffer[2];

    //Console_printf_n_77(console_server_id, "%d stopped at starting point", train->trainID);
    //Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16);
    //if (train_idx == 0)
    Console_printf(console_server_id, "Train %d Detected First Sensor: %s \r\n", Known_Trains[train_idx].id, Start_Sensor);
    //Removed Some Waypoint Stuff + Loop Setup Stuff

    int start_idx = trackNameToIdx(Start_Sensor, Track_B);

    //Reverse back over sensor
    Delay(clock_server_id, 200);
    Send_train_command_byte(uart_server_id, train->trainID, 16+15);
    Delay(clock_server_id, 50);
    //int time_go = dist_go *100/ (Known_Trains[train_idx].speeds[speed_case][1] * 1000) ; //num ticks to go
    Send_train_command_byte(uart_server_id, train->trainID, 16+3);
    if (train->trainID==77)
        Delay(clock_server_id, 250);
    else if (train->trainID==58)
        Delay(clock_server_id, 250);
    else   
        Delay(clock_server_id, 150);
    Send_train_command_byte(uart_server_id, train->trainID, 16);
    Delay(clock_server_id, 200);
    Send_train_command_byte(uart_server_id, train->trainID, 16+15);
    //reverse so going in the right direction again

    Send(sensor_id, finish_localization_msg, 4, sensor_rpl_buffer, 10);
    Console_printf(console_server_id, "Train %d Detected waiting for other train to Localize \r\n", Known_Trains[train_idx].id);
    Send(train_ctrl_id, filler_msg, 4, info_buffer, 10); //msg sent is a dummy msg
    Send_train_command_byte(uart_server_id, train->trainID, 16);
    Delay(clock_server_id, 200);

    uint32_t benchmarking_time = get_current_time();
    //Console_printf(console_server_id, "Train %d starting @ %u \r\n", train->trainID, benchmarking_time);

    bool ListOfSensors[TRACK_MAX] = {0};

    if (track_input == 'b' || track_input == 'a'){
        for (int i = 0; i < TRACK_MAX; i++){
            if (i != 0 && i != 1 && i != 12 && i != 13 && i != 4 && i != 5 && i != 6 && i != 7 && i != 8 && i != 9 && i != 22 && i != 23 && i != 24 && i != 25 && i != 26 && i != 27 && i!= 34 && i != 35 && i < 80){
                ;//ListOfSensors[i] = 0;
            }
            else {
                ListOfSensors[i] = 1;
            }
        }
    }

    ListOfSensors[start_idx] = 1;
    ListOfSensors[Track_B[start_idx].reverse->id] = 1;
    log_sensor(console_server_id, train->trainID, start_idx);


    char Destination[4] = {'C', '1', '0', 0}; //Hardcode destination for now
    int dest_idx = 0;
    bool valid_input = 0;
    //bool firstStepReverse = false;
    bool initialReverse = false;

    train->locationID = start_idx;
    //bookSegment(Track_B, train->trainID, start_idx);
    //printBookings(Track_B, 1, console_server_id);

    while (true) {

        if (!isPaused){
            isPaused = true;
            Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
        }

        //Get user input 
        while(valid_input == 0) {
            Console_printf(console_server_id, "\033[38;1\033[K\033[85;1H");
            Console_printf(console_server_id, "\033[37;1HEnter Goal Sensor for User Train (77) (i.e. A02, E13 etc.):\033[85;1H");
            for (int i = 0; i < 3; i++){
                Destination[i] = Getc(uart_server_id, CONSOLE);
                if (Destination[i] == '\b'){
                    if (i <= 0) i = -1;
                    else{
                        Console_printf(console_server_id, "\033[38;%dH\033[K\033[85;1H", i-1);
                        Destination[i-1] = 'C';
                        i-=2;
                    }
                }
                else {
                    Console_printf(console_server_id, "\033[38;%dH%c\033[85;1H", i, Destination[i]);
                    //uart_printf(CONSOLE, "\033[2;10%dH%x\033[50;1H", i, 'h');//Destination[i]);
                }
            }
            //Validate the input
            if (Destination[0] == 'A' || Destination[0] == 'B' || Destination[0] == 'C' || Destination[0] == 'D' || Destination[0] == 'E') {
                valid_input = 1;
            }
            if (Destination[1] == '0') {
                if (Destination[2] == '1' || Destination[2] == '2' || Destination[2] == '3' || Destination[2] == '4' || Destination[2] == '5' || Destination[2] == '6' || Destination[2] == '7' || Destination[2] == '8' || Destination[2] == '9') {
                    valid_input &= 1;
                }
                else {
                    valid_input = 0;
                }
            }
            else if (Destination[1] == '1') {
                if (Destination[2] == '0' || Destination[2] == '1' || Destination[2] == '2' || Destination[2] == '3' || Destination[2] == '4' || Destination[2] == '5' || Destination[2] == '6') {
                    valid_input &= 1;
                }
                else {
                    valid_input = 0;
                }
            }
            else {
                valid_input = 0;
            }
        } //end of get goal sensor
        valid_input = 0;
        Console_printf(console_server_id, "Goal for Train %d is sensor is %s \r\n", Known_Trains[train_idx].id, Destination);

        dest_idx = trackNameToIdx(Destination, Track_B);

        //Get user input
        int step_size = Dijkstra_User(start_idx, dest_idx, Track_B, sequence, train->trainID, console_server_id, &initialReverse);
        train->headingID = sequence[0].next_sensor_id;
        //Console_printf_n_77(console_server_id, train->trainID, "%d getting started", train->trainID);

        if (initialReverse){
            Delay(clock_server_id, 100);
            Send_train_command_byte(uart_server_id, train->trainID, 16+15);
            initialReverse = false;
        }

        if (isPaused) {
            isPaused = false;
            Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
        }

        //Train 77 uses speed 7, Train 54 uses speed 4
        uint8_t speed_case = 3; 

        uint32_t prev_time = get_current_time();

        // Forced Allocation because we have to book it either way
        // Init Train data
        train->reverse = false;
        train->locationID = start_idx;
        bookSegment(Track_B, train->trainID, start_idx);
        

        int offset_direction = 0;
        int offset_dist = 0;

        // Train Control Logic
        if (step_size > 0){
            train->headingID = sequence[0].next_sensor_id;
        }
        else {
            train->headingID = start_idx;
        }

        bool isStopping = false;
        uint32_t last_go_at = get_current_time();
        int curveOffset = 0;

        // TODO: If the first step goes to itself, just book itself and exit


        //Console_printf_n_77(console_server_id, train->trainID, "Is deadlocked %d", Deadlocked(console_server_id));
        // Look to see if the first section (step) is booked 
        while (bookSection(train->trainID, sequence, 0)){
            if (!isPaused){
                isPaused = true;
                Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
            }
            // Wait for it to clear
            Send_train_command_byte(uart_server_id, train->trainID, 16);
            Console_printf(console_server_id, "Train %d Waiting for Section %s", train->trainID, sequence[0].next_sensor_name);
            if (Deadlocked(console_server_id)){
                printBookings(Track_B, 1, console_server_id); // Track B
                // Replan if possible
                if (train->trainID != train_replanned){
                    //Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);
                    step_size = Dijkstra_User(start_idx, dest_idx, Track_B, sequence, train->trainID, console_server_id, &initialReverse);
                    if (initialReverse){
                        Delay(clock_server_id, 100);
                        Send_train_command_byte(uart_server_id, train->trainID, 16+15);
                        initialReverse = false;
                    }
                    if (step_size == 0){
                        Console_printf(console_server_id, "This is %d, We are stuck!!!!", train->trainID);
                    }
                }
            }
        }

        if (isPaused) {
            isPaused = false;
            Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
        }
        Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[0].next_sensor_name);

        /*
        // Fix the first step reverse
        // seems like first step reverse just reverses on the stop even if it wasnt supposed to be on the spot
        if (sequence[0].needs_reverse && isReverse(sequence[0].next_sensor_id, start_idx)){
            Console_printf(console_server_id, "Train %d Initial Reverse", train->trainID);
            //Console_printf_n_77(console_server_id, train->trainID, "Initial Reverse dist %d", sequence[0].dist_to_sensor);
            //ignore_msg[1] = 0;
            //ignore_msg[2] = 5;
            if (!isPaused){
                isPaused = true;
                Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
            }
            Delay(clock_server_id, 10);
            Send_train_command_byte(uart_server_id, train->trainID, 15);
            Delay(clock_server_id, 10);
            //Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
            last_go_at = get_current_time();
            first_step_rev = true;

            if (step_size > 1){
                train->locationID = sequence[0].next_sensor_id;
                train->headingID = sequence[1].next_sensor_id;
            }
            while (step_size > 1 && bookSection(train->trainID, sequence, 1)){
            if (!isPaused){
                isPaused = true;
                Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
            }
                // Wait for it to clear
                Send_train_command_byte(uart_server_id, train->trainID, 16);
                Console_printf(console_server_id, "Train %d Waiting for Section %s", train->trainID, sequence[1].next_sensor_name);
                if (Deadlocked(console_server_id)){
                    printBookings(Track_B, 1, console_server_id); // Track B
                    // Replan if possible
                    if (train_replanned != train->trainID){
                        //UnbookSection(train->trainID, sequence, 1);
                        Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);
                        step_size = Dijkstra_User(sequence[0].next_sensor_id, dest_idx, Track_B, sequence, train->trainID, console_server_id);
                        if (step_size == 0){
                            Console_printf(console_server_id, "This is %d, We are stuck!!!!", train->trainID);
                        }
                    }
                    // Assume (safe to do so) that step 0 is not occupied
                    bookSection(train->trainID, sequence, 0);
                }
            }
            Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[1].next_sensor_name);
        }
        */

        if (sequence[0].needs_reverse || sequence[0].hard_reverse){
            isStopping = true;
        }

        // Check switches
        if (sequence[0].switch_count > 0){
            setSwitch(uart_server_id, clock_server_id, console_server_id, train->trainID, sequence, 0);
            //Console_printf_n_77(console_server_id, train->trainID, "Train %d Switching %s", train->trainID, sequence[0].next_sensor_name);
        }

        if (isPaused) { //random /unecessary I think
            isPaused = false;
            Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
        }
        Console_printf(console_server_id, "Train 77 db ebtering main \r\n");
        
        Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
        last_go_at = get_current_time();
        uint32_t my_cur_time;

        // UnbookSegment(Track_B, train->trainID, start_idx);

        bool earlyStop = false;
        bool skipSensor = false;
        bool exitEmergencyStop = false;
        // If it's only one step does it unbook the last section? Does it need to?


        Console_printf(console_server_id, "Train 77 db ebtering main fr though \r\n");

        for (int i = 0; i < step_size - 1; i++) {
            /*
            if (first_step_rev){
                i = 1;
                first_step_rev = false;
                bookSegment(Track_B, train->trainID, sequence[0].next_sensor_id);
            }
            */
            // Try booking one step ahead
            train->locationID = sequence[i].next_sensor_id;
            train->headingID = sequence[i+1].next_sensor_id;
            
            Console_printf(console_server_id, "Train 77 db boutta try to book \r\n");
            int count = 0;
            while (bookSection(train->trainID, sequence, i+1)){
                exitEmergencyStop = true;
                // Wait for it to clear
                if (!isPaused){
                    isPaused = true;
                    Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                }
                Send_train_command_byte(uart_server_id, train->trainID, 16);
                Console_printf(console_server_id, "Train %d, emergency stop! \r\n", train->trainID);
                printBookings(Track_B, 1, console_server_id); // Track B
                Delay(clock_server_id, 100);
                count ++;
                Console_printf(console_server_id, "Train %d booking: %s", train->trainID, sequence[i+1].next_sensor_name);
                if (Deadlocked(console_server_id)){
                    // Replan if possible
                    // Replan without booked edges
                    printBookings(Track_B, 1, console_server_id); // Track B
                    Console_printf(console_server_id, "Train %d Deadlocked", train->trainID);

                    // Copy over last step in preparation for releasing
                    next_step lastStep[1];
                    lastStep[0].next_sensor_id = sequence[i].next_sensor_id;
                    lastStep[0].switch_count = sequence[i].switch_count;
                    for (int j = 0; j < lastStep[0].switch_count; j++){
                        lastStep[0].switchID[j] = sequence[i].switchID[j];
                        lastStep[0].switch_dir[j] = sequence[i].switch_dir[j];
                        lastStep[0].switch_name[j] = sequence[i].switch_name[j];
                    }
                    if (train_replanned != train->trainID){
                    Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);
                    int prior_step_idx = 0;
                    if (i == 0){
                        prior_step_idx = start_idx;
                    }
                    else {
                        prior_step_idx = sequence[i-1].next_sensor_id;
                    }
                    step_size = Dijkstra_User(prior_step_idx, dest_idx, Track_B, sequence, train->trainID, console_server_id, &initialReverse);

                    if (initialReverse){
                        Delay(clock_server_id, 100);
                        Send_train_command_byte(uart_server_id, train->trainID, 16+15);
                        initialReverse = false;
                    }
                    
                    if (step_size == 0 || step_size == -1){
                        Console_printf(console_server_id,"This is %d, We are stuck!!!!\n", train->trainID);
                    }
                    else {
                        // Startup Sequence
                        i = 0;
                        // Look to see if the first section (step) is booked 
                        while (bookSection(train->trainID, sequence, 0)){
                            if (Deadlocked(console_server_id)){
                                // Replan if possible
                                Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);
                                int prior_step_idx = 0;
                                if (i == 0){
                                    prior_step_idx = start_idx;
                                }
                                else {
                                    prior_step_idx = sequence[i-1].next_sensor_id;
                                }
                                step_size = Dijkstra_User(prior_step_idx, dest_idx, Track_B, sequence, train->trainID, console_server_id, &initialReverse);
                                if (initialReverse){
                                    Delay(clock_server_id, 100);
                                    Send_train_command_byte(uart_server_id, train->trainID, 16+15);
                                    initialReverse = false;
                                }
                                if (step_size == 0){
                                    Console_printf(console_server_id, "This is %d, We are stuck!!!!", train->trainID);
                                }
                            }
                            // Wait for it to clear
                            Send_train_command_byte(uart_server_id, train->trainID, 16);
                        }
                        //Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[0].next_sensor_name);
                        if (isPaused) {
                            isPaused = false;
                            Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                        }

                        if (sequence[0].switch_count > 0){
                            setSwitch(uart_server_id, clock_server_id, console_server_id, train->trainID, sequence, 0);
                            Console_printf_n_77(console_server_id, train->trainID, "Train %d Switching %s", train->trainID, sequence[0].next_sensor_name);
                        }
                        /*

                        // Fix the first step reverse
                        // seems like first step reverse just reverses on the stop even if it wasnt supposed to be on the spot
                        if (sequence[0].needs_reverse && isReverse(sequence[0].next_sensor_id, start_idx)){
                            //Console_printf_n_77(console_server_id, train->trainID, "Train %d Initial Reverse", train->trainID);
                            //ignore_msg[1] = 1;
                            //ignore_msg[2] = 0;
                            Delay(clock_server_id, 100);
                            Send_train_command_byte(uart_server_id, train->trainID, 15);
                            last_go_at = get_current_time();
                            i = 1; // ignore first step
                        }
                        else if (sequence[0].needs_reverse || sequence[0].hard_reverse){
                            UnbookSection(train->trainID, lastStep, 0);
                            Console_printf(console_server_id, "This should not be the case\r\n");
                            Delay(clock_server_id, 100);
                            Send_train_command_byte(uart_server_id, train->trainID, 15);
                            last_go_at = get_current_time();
                            i = 1; // ignore first step
                            //Console_printf(console_server_id, "This should not be the case\r\n");
                            //isStopping = true;
                        }
                        else {
                            Console_printf(console_server_id, "This should not be the case 2\r\n");
                            UnbookSection(train->trainID, lastStep, 0);
                            Console_printf(console_server_id, "This should not be the case\r\n");
                            Delay(clock_server_id, 100);
                            Send_train_command_byte(uart_server_id, train->trainID, 15);
                            last_go_at = get_current_time();
                            i = 1; // ignore first step
                            //Console_printf_n_77(console_server_id, train->trainID, "Train %d go straight initial", train->trainID);
                            last_go_at = get_current_time();
                            // Need to deal with the case of 1 step
                            
                        }
                        */
                        if (sequence[0].needs_reverse || sequence[0].hard_reverse){
                            isStopping = true;
                        }

                        //UnbookSegment(Track_B, train->trainID, lastStep[0].next_sensor_id);
                        }
                    }
                }
            } //End of booksection
            Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[i+1].next_sensor_name);
            printBookings(Track_B, 1, console_server_id); // Track B

            if (exitEmergencyStop){
                Console_printf(console_server_id, "Exiting emergenecy stop \r\n");
                // Redo startup
                if (isPaused) {
                    isPaused = false;
                    Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                }
                Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
                last_go_at = get_current_time();
                exitEmergencyStop = false;
            }
            
            Console_printf(console_server_id, "Train 77 db before switches \r\n");
            // Set switches
            if (i < step_size - 1) {//check if switches to trip
                setSwitch(uart_server_id, clock_server_id, console_server_id, train->trainID, sequence, i+1);
                //Console_printf_n_77(console_server_id, train->trainID, "Train %d Switching %s", train->trainID, sequence[i+1].next_sensor_name);
            }
            Console_printf(console_server_id, "Train 77 db adter switches \r\n");

            // Check Reverses
            if (isStopping){
                //Console_printf_n_77(console_server_id, train->trainID, "Last go sent %d\r\n", get_current_time() - last_go_at);
                int curveOffset = 1000 * setOffset(sequence[i].next_sensor_id, sequence[i].next_sensor_id);
                int dist_go = sequence[i].dist_to_sensor * 1000;
                Console_printf(console_server_id, "Train 77 db Is stopping \r\n");
                Console_printf_n_77(console_server_id, train->trainID, "Dist go 1 is %d\r\n", dist_go);
                dist_go -= Known_Trains[train_idx].speeds[speed_case][2];
                Console_printf_n_77(console_server_id, train->trainID, "Dist go 2 is %d\r\n", dist_go);
                dist_go -= curveOffset;
                Console_printf_n_77(console_server_id, train->trainID, "Dist go 3 is %d\r\n", dist_go);
                if (train->trainID == 54)
                    dist_go += 1000 * 15; // Hardcode a little offset to make sure it trips
                else
                    dist_go += 1000 * 35;
                Console_printf_n_77(console_server_id, train->trainID, "Dist go 4 is %d\r\n", dist_go);
                
                int time_go = 0;
                // time should enough
                if (dist_go > 0){
                    int time_go = dist_go *100/ (Known_Trains[train_idx].speeds[speed_case][1] * 1000) ; //num ticks to go
                    my_cur_time = get_current_time();
                    Console_printf(console_server_id, "Ea Last go was %u \r\n", my_cur_time - last_go_at);
                    if ((uint32_t)(my_cur_time - last_go_at) < (uint32_t)(500000)){//add an offset if only start moving recently (short moves)
                        //if (i > 0 && setOffset(sequence[i-1].next_sensor_id, sequence[i-1].next_sensor_id))
                        int extra_delay = 1 + ((my_cur_time - last_go_at - 1) / 100000);
                        time_go += 400 - extra_delay * 27 ;//+ extra_delay*2;
                        Console_printf(console_server_id, "Adding Last go Offset \r\n");
                        //time_go += 165;
                    }
                    if (!isPaused){
                        isPaused = true;
                        Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                    }
                    Console_printf(console_server_id, "Case 0, Delay for %d \r\n", time_go);
                    Delay(clock_server_id, time_go);
                    Send_train_command_byte(uart_server_id, train->trainID, 16);

                    int stopping_time = 400; // 3 seconds stopping time
                    //ignore_msg[1] = 4;
                    //ignore_msg[2] = 3;
                    if (!isPaused){
                        isPaused = true;
                        Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                    }

                    Delay(clock_server_id, stopping_time);
                    ListOfSensors[sequence[i].next_sensor_id] = 1;
                    log_sensor(console_server_id, train->trainID, sequence[i].next_sensor_id);
                    if (isPaused) {
                        isPaused = false;
                        Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                    }

                    // need to check soft_reverse
                    // because it seems like it's still checking for it
                    if (sequence[i].needs_reverse || sequence[i].hard_reverse){
                        Send_train_command_byte(uart_server_id, train->trainID, 15);
                        Delay(clock_server_id, 10);
                    }
                    
                    Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
                    last_go_at = get_current_time();
                }
                else { 
                    Console_printf(console_server_id, "You are in a wrong case! Delay %d\r\n", time_go);
                    Send_train_command_byte(uart_server_id, train->trainID, 16);
                }
                isStopping = false; 
                //skipSensor = true; //skip the sensor for this iteration since we should have already stopped on it
            }

            // Send the train forward & sensor detection
            uart_printf(CONSOLE, "\033[32m Train %d going to: %s \033[37m\r\n", train->trainID, sequence[i].next_sensor_name);

            // Stopping Sequence
            if (sequence[i+1].needs_reverse || sequence[i+1].hard_reverse || i == step_size - 2){
                Console_printf(console_server_id, "Train 77 db Early stopping \r\n");
                isStopping = true;
                int curveOffset = 1000 * setOffset(sequence[i+1].next_sensor_id, sequence[i].next_sensor_id);
                int dist_go = sequence[i+1].dist_to_sensor * 1000;
                if (dist_go < Known_Trains[train_idx].speeds[speed_case][2]){
                    ListOfSensors[sequence[i].next_sensor_id] = 1;
                    log_sensor(console_server_id, train->trainID, sequence[i].next_sensor_id);
                    dist_go += 1000*sequence[i].dist_to_sensor; 
                    dist_go -= Known_Trains[train_idx].speeds[speed_case][2];
                    dist_go -= curveOffset;
                    if (train->trainID == 77){
                        dist_go += 1000*60;
                    }
                    int time_go = 0;
                    // Calculate based on stopping distance
                    // if time is enough
                    if (dist_go > 0){
                        time_go = dist_go *100/ (Known_Trains[train_idx].speeds[speed_case][1] * 1000) ; //num ticks to go
                        Console_printf(console_server_id, "Case 1, Delay for %d \r\n", time_go);
                        if (!isPaused){
                            isPaused = true;
                            Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                        }
                        my_cur_time = get_current_time();
                        Console_printf(console_server_id, "Sc Last go was %u \r\n", my_cur_time - last_go_at);
                        if ((my_cur_time - last_go_at) < 500000){//add an offset if only start moving recently (short moves)
                            //if (i > 0 && setOffset(sequence[i-1].next_sensor_id, sequence[i-1].next_sensor_id))
                            int extra_delay = 1 + ((my_cur_time - last_go_at - 1) / 100000);
                            time_go += extra_delay*40 ;//+ extra_delay*2;
                            Console_printf(console_server_id, "Adding Last go Offset \r\n");
                        }
                        
                        Delay(clock_server_id, time_go);
                        Send_train_command_byte(uart_server_id, train->trainID, 16);
                        //Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                    }
                    // stop immediately if time is not enough
                    else{ 
                        Console_printf(console_server_id, "Emergency Stop! \r\n");
                        Send_train_command_byte(uart_server_id, train->trainID, 16);
                        if (!isPaused){
                            isPaused = true;
                            Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                        }
                    }
                    // Since this handles two steps, we skip the first sensor
                    Console_printf(console_server_id, "Train %d, Unbook section %s\n\r", train->trainID, sequence[i].next_sensor_name);
                    UnbookSection(train->trainID, sequence, i-1); 
                    i++;
                    ListOfSensors[sequence[i].next_sensor_id] = 1; //maybe not
                    log_sensor(console_server_id, train->trainID, sequence[i].next_sensor_id);
                    if (i + 1 < step_size){
                        train->headingID = sequence[i+1].next_sensor_id;

                        int stopping_time = 400; // 3 seconds stopping time
                        //ignore_msg[1] = 4;
                        //ignore_msg[2] = 0;
                        //Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                        
                        Delay(clock_server_id, stopping_time);
                        if (sequence[i].needs_reverse || sequence[i].hard_reverse){
                            Send_train_command_byte(uart_server_id, train->trainID, 15);
                        }

                        // This whole thing is a reverse
                        //
                        //
                        //
                        while (bookSection(train->trainID, sequence, i+1)){
                            exitEmergencyStop = true;
                            // Wait for it to clear
                            if (!isPaused){
                                isPaused = true;
                                Send(sensor_id, pause_sensor_msg, 4, sensor_rpl_buffer, 10);
                            }
                            Send_train_command_byte(uart_server_id, train->trainID, 16);
                            Console_printf(console_server_id, "Train %d, emergency stop! \r\n", train->trainID);
                            printBookings(Track_B, 1, console_server_id); // Track B
                            Delay(clock_server_id, 100);
                            count++;
                            Console_printf(console_server_id, "Train %d booking: %s", train->trainID, sequence[i+1].next_sensor_name);
                            if (Deadlocked(console_server_id)){
                                // Replan if possible
                                // Replan without booked edges
                                printBookings(Track_B, 1, console_server_id); // Track B
                                Console_printf(console_server_id, "Train %d Deadlocked, replanning", train->trainID);

                                // Copy over last step in preparation for releasing
                                next_step lastStep[1];
                                lastStep[0].next_sensor_id = sequence[i].next_sensor_id;
                                lastStep[0].switch_count = sequence[i].switch_count;
                                for (int j = 0; j < lastStep[0].switch_count; j++){
                                    lastStep[0].switchID[j] = sequence[i].switchID[j];
                                    lastStep[0].switch_dir[j] = sequence[i].switch_dir[j];
                                    lastStep[0].switch_name[j] = sequence[i].switch_name[j];
                                }
                                if (train_replanned != train->trainID){
                                    Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);

                                    int prior_step_idx = 0;
                                    if (i == 0){
                                        prior_step_idx = start_idx;
                                    }
                                    else {
                                        prior_step_idx = sequence[i-1].next_sensor_id;
                                    }
                                    step_size = Dijkstra_User(prior_step_idx, dest_idx, Track_B, sequence, train->trainID, console_server_id, &initialReverse);

                                    if (initialReverse){
                                        Delay(clock_server_id, 100);
                                        Send_train_command_byte(uart_server_id, train->trainID, 16+15);
                                        initialReverse = false;
                                    }
                                
                                    if (step_size == 0 || step_size == -1){
                                        Console_printf(console_server_id,"This is %d, We are stuck!!!!\n", train->trainID);
                                    }
                                    else {
                                        // Startup Sequence
                                        i = 0;
                                        // Look to see if the first section (step) is booked 
                                        while (bookSection(train->trainID, sequence, 0)){
                                            if (Deadlocked(console_server_id)){
                                                // Replan if possible
                                                Console_printf(console_server_id, "From %s to %s \r\n", Track_B[train->locationID].name, Track_B[dest_idx].name);
                                                step_size = Dijkstra_User(prior_step_idx, dest_idx, Track_B, sequence, train->trainID, console_server_id, &initialReverse);
                                                if (initialReverse){
                                                    Delay(clock_server_id, 100);
                                                    Send_train_command_byte(uart_server_id, train->trainID, 16+15);
                                                    initialReverse = false;
                                                }
                                                if (step_size == 0){
                                                    Console_printf(console_server_id, "This is %d, We are stuck!!!!", train->trainID);
                                                }
                                            }
                                            // Wait for it to clear
                                            Send_train_command_byte(uart_server_id, train->trainID, 16);
                                        }
                                        //Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[0].next_sensor_name);
                                        if (isPaused) {
                                            isPaused = false;
                                            Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                                        }

                                        /*
                                        // Fix the first step reverse
                                        // seems like first step reverse just reverses on the stop even if it wasnt supposed to be on the spot
                                        if (sequence[0].needs_reverse && isReverse(sequence[0].next_sensor_id, start_idx)){
                                            //Console_printf_n_77(console_server_id, train->trainID, "Train %d Initial Reverse", train->trainID);
                                            //ignore_msg[1] = 1;
                                            //ignore_msg[2] = 0;
                                            Delay(clock_server_id, 100);
                                            Send_train_command_byte(uart_server_id, train->trainID, 15);
                                            last_go_at = get_current_time();
                                            i = 1; // ignore first step
                                        }
                                        else if (sequence[0].needs_reverse || sequence[0].hard_reverse){
                                            UnbookSection(train->trainID, lastStep, 0);
                                            Console_printf(console_server_id, "This should not be the case\r\n");
                                            Delay(clock_server_id, 100);
                                            Send_train_command_byte(uart_server_id, train->trainID, 15);
                                            last_go_at = get_current_time();
                                            i = 1; // ignore first step
                                            //isStopping = true;
                                        }
                                        else {
                                            Console_printf(console_server_id, "This should not be the case 2\r\n");
                                            UnbookSection(train->trainID, lastStep, 0);
                                            Delay(clock_server_id, 100);
                                            Send_train_command_byte(uart_server_id, train->trainID, 15);
                                            last_go_at = get_current_time();
                                            i = 1; // ignore first step
                                            //Console_printf_n_77(console_server_id, train->trainID, "Train %d go straight initial", train->trainID);
                                            last_go_at = get_current_time();
                                            // Need to deal with the case of 1 step
                                            
                                        }
                                        */
                                        if (sequence[0].needs_reverse || sequence[0].hard_reverse){
                                            isStopping = true;
                                        }

                                        // Check switches
                                        if (sequence[i].switch_count > 0){
                                            setSwitch(uart_server_id, clock_server_id, console_server_id, train->trainID, sequence, 0);
                                            Console_printf_n_77(console_server_id, train->trainID, "Train %d Switching %s", train->trainID, sequence[0].next_sensor_name);
                                        }
                                        //UnbookSegment(Track_B, train->trainID, lastStep[0].next_sensor_id);
                                    }
                                }
                            }
                        } //End of booksection
                        Console_printf(console_server_id, "Train %d Booked Section %s", train->trainID, sequence[i+1].next_sensor_name);
                        printBookings(Track_B, 1, console_server_id); // Track B

                        if (exitEmergencyStop){
                            Console_printf(console_server_id, "Exiting emergenecy stop \r\n");
                            // Redo startup
                            exitEmergencyStop = false;
                        }

                        if (sequence[i+1].switch_count > 0){
                            setSwitch(uart_server_id, clock_server_id, console_server_id, train->trainID, sequence, 1);
                            Console_printf_n_77(console_server_id, train->trainID, "Train %d Switching %s", train->trainID, sequence[1].next_sensor_name);
                        }

                        if (isPaused) {
                            isPaused = false;
                            Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                        }
                        Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
                        last_go_at = get_current_time();
                    }
                    else{
                        train->headingID = sequence[i].next_sensor_id;
                        earlyStop = true;
                    }
                    if (isPaused) {
                        isPaused = false;
                        Send(sensor_id, ready_sensor_msg, 4, sensor_rpl_buffer, 10);
                    }
                    isStopping = false;
                }
                else if (!sequence[i].needs_reverse && !sequence[i].hard_reverse ){ // if we reversed this iter, we already should have arrived at the sensor
                    Console_printf(console_server_id, "Train 77 db Reverse Sensor \r\n");
                    // Set off a sensor
                    ListOfSensors[sequence[i].next_sensor_id] = 1;
                    log_sensor(console_server_id, train->trainID, sequence[i].next_sensor_id);
                    ListOfSensors[Track_B[sequence[i].next_sensor_id].reverse->id] = 1;

                    if (checkEntry(i)){ 
                        speed_case = 3;
                        Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
                    }
                    else if (checkExit(i)){ 
                        speed_case = 3;
                        Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
                    }
                    
                    // Poll the sensor
                    //Console_printf_n_77(console_server_id, train->trainID, "Going to a sensor \r\n");
                    Send(sensor_id, sequence[i].next_sensor_name, 4, sensor_rpl_buffer, 10);
                }
            }
            else if (!sequence[i].needs_reverse && !sequence[i].hard_reverse ){ // if we reversed this iter, we already should have arrived at the sensor
                /* !isStopping &&  Don't think this is needed becuase is stopping is only set in the other if
                if (train->trainID == 54)
                    Send_train_command_byte(uart_server_id, train->trainID, 16+4);
                else
                    Send_train_command_byte(uart_server_id, train->trainID, 16+5);
                */
                // Set off a sensorain 
                Console_printf(console_server_id, "Train 77 db normal sensor \r\n");
                ListOfSensors[sequence[i].next_sensor_id] = 1;
                ListOfSensors[Track_B[sequence[i].next_sensor_id].reverse->id] = 1;
                log_sensor(console_server_id, train->trainID, sequence[i].next_sensor_id);
            
                if (checkEntry(i)){ 
                    speed_case = 3;
                    Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
                }
                else if (checkExit(i)){ 
                    speed_case = 3;
                    Send_train_command_byte(uart_server_id, train->trainID, 16 + Known_Trains[train_idx].speeds[speed_case][0]);
                }
                // Poll the sensor 
                Send(sensor_id, sequence[i].next_sensor_name, 4, sensor_rpl_buffer, 10);
            }
            //skipSensor = false; //skip sensor will be decided in the next iter

            // Should have reached the dest on step i by now

            // Unbook the last section
            // If we are not hard reversing on the last section, just unbook it
            // If its only two steps it might not release the first one
            if (i > 0){ // && !sequence[i].hard_reverse && !sequence[i].needs_reverse){
                //Receive(&sender_id, server_msg_buffer, 10); //Receive
                //train->locationID = sequence[i].next_sensor_id;
                //Console_printf(console_server_id, "Train %d, Unbook section %s\n\r", train->trainID, sequence[i].next_sensor_name);
                UnbookSection(train->trainID, sequence, i-1); 
                //printBookings(Track_B, 1, console_server_id); // Track B
            }
            else if (i == 0){
                UnbookSegment(Track_B, train->trainID, start_idx);
            }
            if (train->trainID == 77)
                printBookings(Track_B, 1, console_server_id);
            // Otherwise don't

            /*
            else if (i >= step_size - 4){
                int dist_till_stop = 0;
                for (int j = i; j < step_size; j++){
                    dist_till_stop += sequence[j].dist_to_sensor;
                }
                if (dist_till_stop < 1600 || i == step_size - 2){
                    // Slow down the train to 5 speed
                    speed_case = 0;
                    Send_train_command_byte(uart_server_id, Known_Trains[train_idx].id, 16+5);
                }
            }
            */

/*
            if (i > 0) {
                Model_Error[i-1].actual_time = *sensor_time - prev_time;
                Console_printf_n_77(console_server_id, train->trainID, "Actual time: %d \r\n", Model_Error[i-1].actual_time);
                Console_printf_n_77(console_server_id, train->trainID, "Predict time: %d \r\n", Model_Error[i-1].predict_time);
                Console_printf_n_77(console_server_id, train->trainID, "Distance was: %d \r\n", sequence[i-1].dist_to_sensor);
                Model_Error[i-1].time_error = ((int64_t) Model_Error[i-1].actual_time - (int64_t) Model_Error[i-1].predict_time); //ns
                Model_Error[i-1].dist_error = Model_Error[i-1].time_error *  (int64_t) Known_Trains[train_idx].speeds[speed_case][1] / 1000; //ns * mm / s
                Console_printf_n_77(console_server_id, train->trainID, "Error for %s: Time (us): %d | Distance (nm): %d \r\n", sequence[i-1].next_sensor_name, Model_Error[i-1].time_error/1000, Model_Error[i-1].dist_error);
            }
            prev_time = *sensor_time;
            Model_Error[i].predict_time = ((uint64_t)sequence[i].dist_to_sensor * 1000000000 / (uint64_t)Known_Trains[train_idx].speeds[speed_case][2]); //  = nm * 1000 / (mm/s) = us = our clock rate
*/
        }

        isPaused = true;
        Send(sensor_id, done_msg, 4, sensor_rpl_buffer, 10); //force transition regardless
        // This is the last step if not earlystop
        if (!earlyStop){
            int cOffset = 0;

            if (step_size == 1){
                UnbookSegment(Track_B, train->trainID, start_idx);
            }
            // Set off a sensor
            ListOfSensors[sequence[step_size-1].next_sensor_id] = 1;
            ListOfSensors[Track_B[sequence[step_size-1].next_sensor_id].reverse->id] = 1;
            log_sensor(console_server_id, train->trainID, sequence[step_size - 1].next_sensor_id);
            
            if (step_size > 1){
                cOffset = 1000 * setOffset(sequence[step_size - 1].next_sensor_id, sequence[step_size - 2].next_sensor_id);
                UnbookSection(train->trainID, sequence, step_size - 2); 
                printBookings(Track_B, 1, console_server_id);
            }
            else 
                cOffset = 1000 * setOffset(sequence[step_size - 1].next_sensor_id, sequence[step_size - 1].next_sensor_id);
            int dist_go = sequence[step_size - 1].dist_to_sensor * 1000;
            my_cur_time = get_current_time();
            Console_printf(console_server_id, "End Last go was %u \r\n", my_cur_time - last_go_at);
            if ((uint32_t)(my_cur_time - last_go_at) < (uint32_t)(500000)){//add an offset if only start moving recently (short moves)
                //if (i > 0 && setOffset(sequence[i-1].next_sensor_id, sequence[i-1].next_sensor_id))
                //int extra_delay = 1 + ((my_cur_time - last_go_at - 1) / 100000);
                //time_go += extra_delay*40 + extra_delay*2;
                //Console_printf(console_server_id, "Adding Last go Offset \r\n");
                //time_go += 165;
                dist_go = dist_go*2;
            }
            //Console_printf_n_77(console_server_id, train->trainID, "Step 1, dist_go %d \r\n", dist_go);
            dist_go -= Known_Trains[train_idx].speeds[speed_case][2];
            //Console_printf_n_77(console_server_id, train->trainID, "Step 2, dist_go %d \r\n", dist_go);
            if (train->trainID == 54)
                dist_go -= cOffset;
            //Console_printf_n_77(console_server_id, train->trainID, "Step 3, dist_go %d \r\n", dist_go);
            dist_go += 1000 * 10; // Hardcode a little offset to make sure it trips
            //Console_printf_n_77(console_server_id, train->trainID, "Step 4, dist_go %d \r\n", dist_go);
            int time_go = dist_go *100/ (Known_Trains[train_idx].speeds[speed_case][1] * 1000) ; //num ticks to go
            /*    
            if ((my_cur_time - last_go_at) < 1000000){//add an offset if only start moving recently (short moves)
                //if (i > 0 && setOffset(sequence[i-1].next_sensor_id, sequence[i-1].next_sensor_id))
                int extra_delay = 1 + ((my_cur_time - last_go_at - 1) / 100000);
                time_go += extra_delay*40;// + extra_delay*2;
                Console_printf(console_server_id, "Adding Last go Offset \r\n");
                time_go += 165;
                //dist_go = dist_go*2;
            }             
            */
            // time should enough
            if (dist_go > 0){
                //Console_printf_n_77(console_server_id, train->trainID, "Case 0, Delay for %d \r\n", time_go);
                Delay(clock_server_id, time_go);
                Send_train_command_byte(uart_server_id, train->trainID, 16);
            }
            else { 
                //Console_printf_n_77(console_server_id, train->trainID, "Case 1, Immediate Stop! Should not happen. Delay %d\r\n", time_go);
                Send_train_command_byte(uart_server_id, train->trainID, 16);
            }
            int stopping_time = 300; // 3 second stopping time 
            Delay(clock_server_id, stopping_time);
        }
        // Try to go back maybe?
        for (int i = 0; i < sequence[step_size - 1].switch_count; i++){
            UnbookSegment(Track_B, train->trainID, sequence[step_size-1].switchID[i]);
        }
        printBookings(Track_B, 1, console_server_id);

        Console_printf(console_server_id, "Finished Train %d iteration \r\n", Known_Trains[train_idx].id);
        int temp_idx = start_idx;

        //Make sure we provide the right side of the switch as our starting point
        if (sequence[step_size - 1].next_sensor_id == Track_B[dest_idx].reverse->id)
            start_idx = Track_B[dest_idx].reverse->id;
        else
            start_idx = dest_idx;
        
        dest_idx = temp_idx;
        Delay(clock_server_id, 100);

    }

    Send(train_ctrl_id, filler_msg, 4, sensor_rpl_buffer, 4); //msg sent is a dummy msg
    Exit();

}
