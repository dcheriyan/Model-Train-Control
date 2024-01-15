#include "tasks.h"
#include "util.h"
#include "kernel.h"
#include <stdint.h>
#include <stdarg.h>
#include "rpi.h"

/*************** GENERAL *****************/
void Yield(){
    asm volatile("SVC 1");
}

int Create(int priority, void (*function)()){
    asm volatile("SVC 2");
    
    int child_tid; 
    asm volatile("mov %[zzz], x0" : [zzz] "=r"(child_tid));
    return child_tid;
}

void Exit(){
    asm volatile("SVC 3");

    // You screwed something up if you see me
    uart_printf(CONSOLE, "panic kill!");
}

int MyTid(){
    uint64_t myTid = 0;

    asm volatile("SVC 4");
    asm volatile("mov %[zzz], x0" : [zzz] "=r"(myTid));
    return myTid;
}

int MyParentTid(){
    uint64_t parentTid = 0;
    asm volatile("SVC 5");
        
    asm volatile("mov %[zzz], x0" : [zzz] "=r"(parentTid)); 
    return parentTid;
    // It is 0 if it is an orphan. The kernel will adopt all of you.
}

/*************** MESSAGING *****************/
int Send(int tid, const char *msg, int msglen, char *reply, int replylen){
    asm volatile ("SVC 6");
    
    int reply_len; 
    asm volatile("mov %[zzz], x0" : [zzz] "=r"(reply_len));
    return reply_len;
}
int Receive(int *tid, char *msg, int msglen){
    asm volatile ("SVC 7");
    
    int receive_len; 
    asm volatile("mov %[zzz], x0" : [zzz] "=r"(receive_len));
    return receive_len;
}
int Reply( int tid, void *reply, int replylen){
    asm volatile ("SVC 8");

    int reply_len; 
    asm volatile("mov %[zzz], x0" : [zzz] "=r"(reply_len));
    return reply_len;
}

/*************** Name Server *****************/
//Assume input name is formatted: "RMy_Name" (i.e. has a R prepended)
int RegisterAs(const char *name) {
    int name_len = cust_strlen(name);
    char reply_buf[4] = "000";
    int send_status = 0;
    //uart_printf(CONSOLE, "Register as sending name: %s \r\n", name);

    send_status = Send(NAME_SERVER_TID, name, name_len, reply_buf, 5);

    //uart_printf(CONSOLE, "Register As Return buff was %s \r\n", reply_buf);

    if (send_status > -1) {
        return 0;
    }

    return -1;
}

int WhoIs(const char *name) {
    int name_len = cust_strlen(name);
    int found_id = -1;
    char reply_buf[3] = {0};
    int send_status;

    //uart_printf(CONSOLE, "Who is sending name: %s \r\n", name);
    send_status = Send(NAME_SERVER_TID, name, name_len, reply_buf, 3);

    //uart_printf(CONSOLE, "Who is Return buff was %s \r\n", reply_buf);

    if (send_status > -1) {
        found_id = reply_buf[0];
        return found_id;
    }
    
    return -1;
}

/*************** Interrupts *****************/
int AwaitEvent(int eventid){
    asm volatile ("SVC 9");
    /*  Kernel will decide if valid
    if (eventid != 97) // currently only 97 is mapped
        return -1;
    asm volatile("mov x0, %[zzz]" : [zzz] "=r"(eventid));
    */

    int retval; 
    asm volatile("mov %[zzz], x0" : [zzz] "=r"(retval));
    return retval;
}

/*************** Clock *****************/
int Time(int tid) {
    int return_val = 0;
    char Time_msg[5] = "Time";
    char reply_buf[10] = {0};
    int send_status = Send(tid, Time_msg, 5, reply_buf, 10);
    if (send_status < 0){
        return -1;
    }

    return_val = *((int *) reply_buf);
    return return_val;
}

int Delay(int tid, int ticks){
    if (ticks < 0) {
        return -2;
    }

    int return_val = 0;
    char Delay_msg[10] = {0};
    Delay_msg[0] = 'D';

    int mod_ticks = ticks;
    char cur_char = 0;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    Delay_msg[4] = cur_char;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    Delay_msg[3] = cur_char;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    Delay_msg[2] = cur_char;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    Delay_msg[1] = cur_char;
    /*
    int encoded_ticks = Delay_msg[1]*(255^3) + Delay_msg[2]*(255^2) + Delay_msg[3]*(255) + Delay_msg[4];
    uart_printf(CONSOLE, "Encoded ticks is %d \r\n", encoded_ticks);
    */
    char reply_buf[10] = {0};
    //uart_printf(CONSOLE, "Sending Delay %s, %d \r\n", Delay_msg, *((int *)(Delay_msg+1)));
    int send_status = Send(tid, Delay_msg, 10, reply_buf, 10);
    if (send_status < 0){
        return -1;
    }

    return_val = *((int *) reply_buf);
    if (return_val < 0) {
        return -2;
    }
    return return_val;

}

int DelayUntil(int tid, int ticks){
    int return_val = 0;
    char Delay_msg[10] = {0};
    Delay_msg[0] = 'U';

    int mod_ticks = ticks;
    char cur_char = 0;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    Delay_msg[4] = cur_char;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    Delay_msg[3] = cur_char;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    Delay_msg[2] = cur_char;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    Delay_msg[1] = cur_char;

    char reply_buf[10] = {0};
    int send_status = Send(tid, Delay_msg, 10, reply_buf, 10);
    if (send_status < 0){
        return -1;
    }

    return_val = *((int *) reply_buf);
    if (return_val < 0) {
        return -2;
    }
    return return_val;

}

/************** Miscellaneous *******************/
uint32_t getInitTime(){
    uint32_t initTime;

    asm volatile("SVC 10");
    asm volatile("mov %[zzz], x0" : [zzz] "=r"(initTime));
    //uart_printf(CONSOLE, "\r\n Idling %u %% of the time!", idle_usage);
    
    return initTime;
}

int Idle_time_inq(){
    int idle_usage = 0;
    asm volatile("SVC 11");
    asm volatile("mov %[zzz], x0" : [zzz] "=r"(idle_usage));
    //uart_printf(CONSOLE, "\r\n Idling %u %% of the time!", idle_usage);
    
    return idle_usage;
}


/******************* IO SERVER ************************/
int Getc(int tid, int channel) {
    int return_val = 0;
    char Get_msg[10] = {0};
    Get_msg[0] = 'G';
    char line = channel; //line is only 1 or 2, easily stored in a char
    Get_msg[1] = line;
//    int2char_arr(channel, Get_msg, 1);

    char reply_buf[10] = {0};
    //uart_printf(CONSOLE, "Sending Getc \r\n");
    int send_status = Send(tid, Get_msg, 10, reply_buf, 10);
    if (send_status < 0){
        return -1;
    }

    return_val = reply_buf[0];

    return return_val;
}

int Putc(int tid, int channel, unsigned char ch) {
    int return_val = 0;
    char Put_msg[12] = {0};
    Put_msg[0] = 'P';
    char line = channel; //line is only 1 or 2, easily stored in a char
    Put_msg[1] = line;
    Put_msg[2] = ch;
    //uart_printf(CONSOLE, "\033[35m Putc sending message %x \033[37m \r\n", ch );
    //int2char_arr(channel, Put_msg, 2);
    //uart_printf(CONSOLE, "\033[35m After thing \033[37m" );

    char reply_buf[10] = {0};
    //uart_printf(CONSOLE, "Sending Putc \r\n");
    int send_status = Send(tid, Put_msg, 12, reply_buf, 10);
    
    //uart_printf(CONSOLE, "\033[35m Send unblocked\033[37m \r\n", ch );

    if (send_status < 0){
        //uart_printf(CONSOLE, "\033[35m NO \033[37m \r\n", ch );
        return -1;
    }
    //uart_printf(CONSOLE, "\033[35m Putc sfinished\033[37m \r\n", ch );
    
    return 0;
}

int Puts(int tid, int channel, char *mystr) {
    int return_val = 0;
    char Put_msg[12] = {0};
    Put_msg[0] = 'S';
    char line = channel; //line is only 1 or 2, easily stored in a char
    Put_msg[1] = line;
    int i = 0;
    for (; (mystr[i] != 255) && (i < 9); i++){
        Put_msg[2+i] = mystr[i];
    }
    Put_msg[2+i] = 255;
/*
    uart_printf(CONSOLE, "\033[35m Puts sending message ");
    int j = 0;
    for(; Put_msg[j] != 255; j++) {
        uart_printf(CONSOLE, "%d ", Put_msg[j]);
    }
    uart_printf(CONSOLE, "%d ", Put_msg[j]);
    uart_printf(CONSOLE, " \033[37m\r\n" );
*/ 
    //int2char_arr(channel, Put_msg, 2);
    //uart_printf(CONSOLE, "\033[35m After thing \033[37m" );

    char reply_buf[10] = {0};
    //uart_printf(CONSOLE, "Sending Putc \r\n");
    int send_status = Send(tid, Put_msg, 12, reply_buf, 10);
    
    //uart_printf(CONSOLE, "\033[35m Send unblocked\033[37m \r\n", ch );

    if (send_status < 0){
        //uart_printf(CONSOLE, "\033[35m NO \033[37m \r\n", ch );
        return -1;
    }
    //uart_printf(CONSOLE, "\033[35m Putc sfinished\033[37m \r\n", ch );
    
    return 0;
}

int RX_Read_Marklin(){
    int rx_value = -1;
    asm volatile("SVC 12");
    asm volatile("mov %[zzz], x0" : [zzz] "=r"(rx_value));
    //uart_printf(CONSOLE, "\r\n Idling %u %% of the time!", idle_usage);
    
    return rx_value;
}

/******* Console limited printf *******/
// printf-style printing, with limited format support
static void format_print_string (char* buffer, char *fmt, va_list va ) {
	char bf[12] = {0};
	char ch;
    int index = 0;

  while ( ( ch = *(fmt++) ) ) {
		if ( ch != '%' ) {
            buffer[index] = ch;
            index++;
			//uart_putc(line, ch );
        }
		else {
			ch = *(fmt++);
			switch( ch ) {
			case 'u':
				ui2a( va_arg( va, unsigned int ), 10, bf );
                for (int j = 0; bf[j] != 0; j++){
                    buffer[index] = bf[j];
                    index++;
                }
				break;
			case 'd':
				i2a( va_arg( va, int ), bf );
				for (int j = 0; bf[j] != 0; j++){
                    buffer[index] = bf[j];
                    index++;
                }
				break;
			case 'x':
				ui2a( va_arg( va, unsigned int ), 16, bf );
				for (int j = 0; bf[j] != 0; j++){
                    buffer[index] = bf[j];
                    index++;
                }
				break;
			case 's':
                const char* buf = va_arg( va, char* );
                while (*buf) {
                    buffer[index]= *buf;
                    buf++;
                    index++;
                }
				break;
			case '%':
				buffer[index] = ch;
                index++;
                break;
            case 'c':
                char c_buf = va_arg( va, int);
                //uart_putc(CONSOLE, c_buf);
				buffer[index] = c_buf;
                index++;
                break;
            case '\0':
                return;
			}
		}
        

	}
}

void Console_printf_n_77(int tid, int train, char *fmt, ... ) {
    if (train != 77) {
        char Put_msg[102] = {0};
        Put_msg[0] = 'S';
        char line = CONSOLE;
        Put_msg[1] = line;

        va_list va;
        va_start(va,fmt);
        format_print_string( Put_msg + 2, fmt, va );
        va_end(va);

        char reply_buf[10] = {0};
        int send_status = Send(tid, Put_msg, 102, reply_buf, 10);
    }
}

void Console_printf(int tid, char *fmt, ... ) {
    
        char Put_msg[102] = {0};
        Put_msg[0] = 'S';
        char line = CONSOLE;
        Put_msg[1] = line;

        va_list va;
        va_start(va,fmt);
        format_print_string( Put_msg + 2, fmt, va );
        va_end(va);

        char reply_buf[10] = {0};
        int send_status = Send(tid, Put_msg, 102, reply_buf, 10);
}

void Console_printf_spec(int tid, char *fmt, ... ) {
        char Put_msg[102] = {0};
        Put_msg[0] = 'S';
        char line = CONSOLE;
        Put_msg[1] = line;

        va_list va;
        va_start(va,fmt);
        format_print_string( Put_msg + 2, fmt, va );
        va_end(va);

        char reply_buf[10] = {0};
        int send_status = Send(tid, Put_msg, 51, reply_buf, 10);
}
