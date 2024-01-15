#include "rpi.h"
#include "util.h"
#include "interrupts.h"
#include <stdbool.h>
#include "a0.h"
#include "tasks.h"

#define MINUTE_POS 0
#define SECOND_POS 1
#define DECI_SECOND_POS 2
#define RAW_SECOND_POS 3

extern char switch_data[NUM_SWITCHES][2];

/************ TIMER ************/
void update_display_time_value(uint32_t start_time, uint32_t *time_array) {
    uint32_t raw_time = 0;
    raw_time = get_current_time();

    uint32_t deci_second = (raw_time - start_time) / 100000;
    uint32_t second = deci_second / 10;
    uint32_t minute = second / 60;
    second %= 60;
    deci_second %= 10;

    time_array[MINUTE_POS] = minute;
    time_array[SECOND_POS] = second;
    time_array[DECI_SECOND_POS] = deci_second;
    time_array[RAW_SECOND_POS] = raw_time;
}

//pad with 0s as necessary so it looks nice and doesn't move around on screen
/* Timer Format: " < Minutes 4 digits, pad with zeroes >:< Seconds 2 digits, pad with zeros >:< deci second one digit> " */
void print_formatted_time_line(uint32_t *time_array) {
    //char timer_string[] = "Time Since Start";
    char minute_string[] = "0000";
    char second_string[] = "00";
    char deci_second_string[] = "0";
    
    if (time_array[MINUTE_POS] >= 1000) {
        //no padding
        ui2a( time_array[MINUTE_POS], 10, minute_string);
    }
    else if (time_array[MINUTE_POS] >= 100) {
        // 1 digit of padding
        ui2a( 0, 10, minute_string);
        ui2a( time_array[MINUTE_POS], 10, minute_string+ 1);
    }
    else if (time_array[MINUTE_POS] >= 10) {
        // 2 digit of padding
        ui2a( 0, 10, minute_string);
        ui2a( 0, 10, minute_string + 1);
        ui2a( time_array[MINUTE_POS], 10, minute_string + 2);
    }
    else {
        // 3 digit of padding
        ui2a( 0, 10, minute_string);
        ui2a( 0, 10, minute_string + 1);
        ui2a( 0, 10, minute_string + 2);
        ui2a( time_array[MINUTE_POS], 10, minute_string + 3);
    }

    if (time_array[SECOND_POS] >= 10) {
        // no padding
        ui2a( time_array[SECOND_POS], 10, second_string);
    }
    else {
        // 1 digit of padding
        ui2a( 0, 10, second_string);
        ui2a( time_array[SECOND_POS], 10, second_string + 1);
    }

    ui2a( time_array[DECI_SECOND_POS], 10, deci_second_string);

    uart_printf(CONSOLE, "\033[1;8H\033[K  %s:%s:%s\033[85;1H", minute_string, second_string, deci_second_string);
}

/**************************  SWITCHES  ******************************************/
uint8_t Set_branch(int uart_server, uint8_t total_switches, uint8_t switch_id) {
    //uart_printf(CONSOLE, "\033[%u;1H \033[35m In switch curve. switch # %x, switch state %x \033[37m ", DEBUG_LINE_2, switch_id);    //Guard Data
    uint8_t found = 0;
    uint8_t switch_idx = 0;
    for(int i = 0; (i < total_switches) && (found == 0); i++) {
        if (switch_data[i][0] == switch_id) {
            //uart_printf(CONSOLE, "\033[%u;4H%u", SWITCH_LINE + i + 1, switch_data[i][0]);
            //uart_printf(CONSOLE, "\033[%u;21Hc", SWITCH_LINE + i + 1);
            switch_data[i][1] = 'c';
            found = 1;
        }

    }
    if (found == 0) {
        return 0;
    }

    //uart_printf(CONSOLE, "\033[%u;1H\033[35m Setting branch", DEBUG_LINE_2);
    Putc(uart_server, MARKLIN, 34); //Branch
    Putc(uart_server, MARKLIN, switch_id);

    return 1;
}

uint8_t Set_straight(int uart_server,  uint8_t total_switches, uint8_t switch_id) {
    //uart_printf(CONSOLE, "\033[%u;1H \033[35m In switch straight. switch # %x, switch state %x \033[37m ", DEBUG_LINE_2, switch_id);
    //Guard Data
    uint8_t found = 0;
    uint8_t switch_idx = 0;
    for(int i = 0; (i < total_switches) && (found == 0); i++) {
        if (switch_data[i][0] == switch_id) {
            //uart_printf(CONSOLE, "\033[%u;4H%u", SWITCH_LINE + i + 1, switch_data[i][0]); //Position Cursor on line
            //uart_printf(CONSOLE, "\033[%u;21Hs", SWITCH_LINE + i + 1);
            switch_data[i][1] = 's';
            found = 1;
        }

    }
    if (found == 0) {
        return 0;
    }

    //uart_printf(CONSOLE, "\033[%u;1H\033[35m Setting straight\033[37m", DEBUG_LINE_2);
    Putc(uart_server, MARKLIN, 33); //Straight
    Putc(uart_server, MARKLIN, switch_id);

    return 1;
}

uint8_t Turn_switch_off(int uart_server) {
    Putc(uart_server, MARKLIN, 32); //solenoid off
    //uart_printf(CONSOLE, "\033[%u;1H\033[35m Setting off\033[37m", DEBUG_LINE_2+1);
    return 1;
}

//Takes max switches
void Print_switch_info(uint8_t console_line, uint8_t total_switches) { 
    uart_printf(CONSOLE, "\033[%u;1H", console_line); //Move Cursor to next line
    uart_puts(CONSOLE, "\033[K" ); //clear line
    uart_puts(CONSOLE, "| Switch # | Current State |" ); //info

    for(uint8_t i = 0; i < total_switches; i++) {
        uart_printf(CONSOLE, "\033[%u;1H", ++console_line); //Move Cursor to next line
        uart_puts(CONSOLE, "\033[K" ); //clear line
        //uart_putc(uart_line, '|' ); //art :)
        uart_printf(CONSOLE, "\033[%u;4H", console_line); //Position Cursor on line
        uart_printf(CONSOLE, "%u", switch_data[i][0]);
        //uart_printf(uart_line, "\033[%u;12H", console_line); //Position Cursor on line
        //uart_putc(uart_line, '|' ); //art :)
        uart_printf(CONSOLE, "\033[%u;21H", console_line); //Position Cursor on line
        uart_putc(CONSOLE, switch_data[i][1]);
        //uart_printf(uart_line, "\033[%u;28H", console_line); //Position Cursor on line
        //uart_putc(uart_line, '|' ); //art :)
    }

}

/****************************  TRAINS  ******************************************/

// Send a command to tell a train to: change it's speed, turn the headlight on, or reverse direction
uint8_t Send_train_command_byte(int uart_server, uint8_t train_id, uint8_t train_command) {
    char train_cmd[3] = {train_command, train_id, 255};
    //uart_printf(CONSOLE, "Command is %x", train_cmd[0]);
    //uart_printf(CONSOLE, "Command is %x", train_cmd[1]);
    Puts(uart_server, MARKLIN, train_cmd);

    return 1;
}

/****************************  SENSORS  ******************************************/
uint8_t Request_Sensor_Data(int uart_server) {
    Putc(uart_server, MARKLIN, 128+5); //Read in all banks
    return 1;
}

uint8_t Update_sensor_buffer(char triggered_sensor_buffer[NUM_RECENT_SENSOR][4], char* sensor_name) {
    // If no space move the previous values down the array
    for (int i = 3; i >= 0; i--){
        memcpy(triggered_sensor_buffer[i+1], triggered_sensor_buffer[i], 4);
    }

    //Added the newest value to the top
    memcpy(triggered_sensor_buffer[0], sensor_name, 4);
    return 1;
}

void Generate_sensor_name(char Bank, uint8_t value, char* sensor_name_string) {
    char misc_chars[2] = {Bank, '0'};
    char temp_sensor_number[3] = {' ', ' ', ' '};
    if (value < 10) {
        ui2a( value, 10, temp_sensor_number);
        memcpy(sensor_name_string, misc_chars, 1);
        memcpy(sensor_name_string + 1, misc_chars + 1, 1);
        memcpy(sensor_name_string + 2, temp_sensor_number, 1);
    }
    else {
        ui2a( value, 10, temp_sensor_number);
        memcpy(sensor_name_string, misc_chars, 1);
        memcpy(sensor_name_string + 1, temp_sensor_number, 2);

    }
}

#define BITMASK_8 0x80 //0b10000000
#define BITMASK_7 0x40 //0b01000000
#define BITMASK_6 0x20 //0b00100000
#define BITMASK_5 0x10 //0b00010000
#define BITMASK_4 0x8  //0b00001000
#define BITMASK_3 0x4  //0b00000100
#define BITMASK_2 0x2  //0b00000010
#define BITMASK_1 0x1  //0b00000001

//only store 5 most recently triggered sensors
void Process_sensor_data(uint8_t raw_value, char Bank, bool upper, char triggered_sensor_buffer[NUM_RECENT_SENSOR][4]) {
    char sensor_name[4] = {' ', ' ', ' ', '\0'};
    if (raw_value & BITMASK_8) {
        Generate_sensor_name(Bank, (1 + 8*upper), sensor_name);
        Update_sensor_buffer(triggered_sensor_buffer, sensor_name);
    }
    if (raw_value & BITMASK_7) {
        Generate_sensor_name(Bank, (2 + 8*upper), sensor_name);
        Update_sensor_buffer(triggered_sensor_buffer, sensor_name);
    }
    if (raw_value & BITMASK_6) {
        Generate_sensor_name(Bank, (3 + 8*upper), sensor_name);
        Update_sensor_buffer(triggered_sensor_buffer, sensor_name);
    }
    if (raw_value & BITMASK_5) {
        Generate_sensor_name(Bank, (4 + 8*upper), sensor_name);
        Update_sensor_buffer(triggered_sensor_buffer, sensor_name);
    }
    if (raw_value & BITMASK_4) {
        Generate_sensor_name(Bank, (5 + 8*upper), sensor_name);
        Update_sensor_buffer(triggered_sensor_buffer, sensor_name);
    }
    if (raw_value & BITMASK_3) {
        Generate_sensor_name(Bank, (6 + 8*upper), sensor_name);
        Update_sensor_buffer(triggered_sensor_buffer, sensor_name);
    }
    if (raw_value & BITMASK_2) {
        Generate_sensor_name(Bank, (7 + 8*upper), sensor_name);
        Update_sensor_buffer(triggered_sensor_buffer, sensor_name);
    }
    if (raw_value & BITMASK_1) {
        Generate_sensor_name(Bank, (8 + 8*upper), sensor_name);
        Update_sensor_buffer(triggered_sensor_buffer, sensor_name);
    }

}
