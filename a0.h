#ifndef _a0_h_
#include <stdint.h>
#define _a0_h_ 1

//Track info
#define NUM_SWITCHES 22
#define NUM_RECENT_SENSOR 5 //  Amount of most recent sensor values to store

//Some macros to help manage the cursor lines
#define SWITCH_LINE 7
#define DEBUG_LINE 80 //Where to print debug messages

void update_display_time_value(uint32_t start_time, uint32_t *time_array);

void print_formatted_time_line(uint32_t *time_array);

uint8_t Send_train_command_byte(int uart_server, uint8_t train_id, uint8_t train_command);

uint8_t Set_straight(int uart_server, uint8_t total_switches, uint8_t switch_id);

uint8_t Set_branch(int uart_server, uint8_t total_switches, uint8_t switch_id);

uint8_t Turn_switch_off(int uart_server);

uint8_t Request_Sensor_Data(int uart_server);

void Process_sensor_data(uint8_t raw_value, char Bank, bool upper, char triggered_sensor_buffer[NUM_RECENT_SENSOR][4]);

#endif //a0.h
