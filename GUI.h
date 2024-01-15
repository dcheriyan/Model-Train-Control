#ifndef _GUI_h_
#define _GUI_h_ 1

#include <stdint.h>

#define USER_TRAIN 77
#define CPU_TRAIN 54
#define NUM_GOAL_SENSORS 80

uint8_t log_sensor(int console_server_id, int my_train_id, int sensor_idx);

void Update_sensors_GUI(int sensor, int train, char user_train_sensors[NUM_GOAL_SENSORS], char cpu_train_sensors[NUM_GOAL_SENSORS]);

void setup_GUI(int console_server_id);

//Testing
void set_all_A(int console_server_id, uint8_t state);
void set_all_B(int console_server_id, uint8_t state);
void set_all_C(int console_server_id, uint8_t state);
void set_all_D(int console_server_id, uint8_t state);
void set_all_E(int console_server_id, uint8_t state);
void set_Dummy_sensors(int console_server_id, uint8_t state);
void set_Dummy_messages(int console_server_id);

#endif //GUI.h