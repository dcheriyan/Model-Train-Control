#ifndef __TRAIN_H_
#define __TRAIN_H_
#include <stdbool.h>

#include <stdint.h>

#define NUM_CHAR_TRAIN 2
#define NUM_CHAR_VEL 5

typedef struct Train_info{
    uint8_t id;
    uint64_t speeds[NUM_CHAR_VEL][3];
} Train_info;

typedef struct Error_Tracker{
    uint32_t predict_time;
    uint32_t actual_time;
    int64_t dist_error;
    int64_t time_error;
} Error_Tracker;

typedef struct Train Train;
struct Train{
    int trainID;
    int locationID;
    int headingID;
    bool reverse;
};
void Euler_Train_Task();
void User_Train_Task();

bool isReverse(int idx1, int idx2);
#endif
