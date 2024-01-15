#ifndef _tasks_h_
#define _tasks_h_ 1

#include <stdint.h>
// General Kernel Interaction
int Create(int priority, void (*function)());
int MyTid();
int MyParentTid();
void Yield();
void Exit();

// Sending Messages
int Send(int tid, const char *msg, int msglen, char *reply, int replylen);
int Receive(int *tid, char *msg, int msglen);
int Reply( int tid, void *reply, int replylen);

//Name Server API
int RegisterAs(const char *name);
int WhoIs(const char *name);

//Interrupts
int AwaitEvent(int eventid);

//Clock
int Time(int tid);
int Delay(int tid, int ticks);
int DelayUntil(int tid, int ticks);

//Misc
uint32_t getInitTime();
int Idle_time_inq();

//IO Server
int Getc(int tid, int channel);
int Putc(int tid, int channel, unsigned char ch);

int Puts(int tid, int channel, char *mystr) ;
void Console_printf( int tid, char *fmt, ... );
void Console_printf_spec( int tid, char *fmt, ... );
void Console_printf_n_77(int tid, int train, char *fmt, ... );

#endif
