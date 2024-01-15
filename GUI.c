#include "rpi.h"
#include "util.h"
#include "interrupts.h"
#include <stdbool.h>
#include "a0.h"
#include "tasks.h"
#include "track.h"

#include "GUI.h"

enum sensor_state {
    unvisited,
    user_train,
    cpu_train,
    both_train
};

uint8_t log_sensor(int console_server_id, int my_train_id, int sensor_idx) {
     int return_val = 0;
     char GUI_msg[5] = {0};
     GUI_msg[0] = 'T';
     GUI_msg[1] = sensor_idx;
     GUI_msg[2] = my_train_id;

     char reply_buf[10] = {0};
     //uart_printf(CONSOLE, "Sending Putc \r\n");
     int send_status = Send(console_server_id, GUI_msg, 5, reply_buf, 10);

     //uart_printf(CONSOLE, "\033[35m Send unblocked\033[37m \r\n", ch );

     if (send_status < 0){
          //uart_printf(CONSOLE, "\033[35m NO \033[37m \r\n", ch );
          return -1;
     }
     //uart_printf(CONSOLE, "\033[35m Putc sfinished\033[37m \r\n", ch );

     return 0;

}

void setup_GUI(int console_server_id) {
     char Line_1[] = "Clock: \r\n";
     char Line_2[] = "\r\n";//"Idle Time: ___  \r\n";
     char Line_3[] = " \r\n";
     
     char Line_4[] = "2 Trains are Competing to visit all the Sensors on the Track (marked on UI with 'O'). One is Controlled by the Computer, and the other by a User via inputted Goal sensors \r\n";
     char Line_5[] =  "LEGEND: '\033[33mO\033[37m' : Unvisted ; '\033[36mO\033[37m' Visited by User ONLY ; '\033[35mO\033[37m' Visited by Computer ONLY ; '\033[34mO\033[37m' Visited by Both \r\n";
     char Line_6[] = " \r\n";
     char Line_7[] = " \r\n";
     char Line_8[] = "                                                \033[33m C13/14            E7/8 \033[37m \r\n";
     char Line_9[] = "        ==============/=============/==============O================O===============\\\\ \r\n";
     char Line_10[] = "                     //		  //                                                 \\\\ \r\n";
     char Line_11[] = "        =============/           //                                                   O \033[33m D7/8 \033[37m \r\n";
     char Line_12[] = "                   //           // \033[33m C11/12         B5/6   D3/4         E5/6  D5/6   \033[37m  || \r\n";
     char Line_13[] = "                  //           /=====O==========\\====O=====O=====/=====O=====O========\\|  \r\n";
     char Line_14[] = "                 //           //                \\\\              //                    || \r\n";
     char Line_15[] = "                //           //     \033[33m      E15/16  O             O   E3/4   \033[37m           || \r\n";
     char Line_16[] = "               //           //                     \\\\    |    //                      || \r\n";
     char Line_17[] = "   \033[33m   A15/16   O      A3/4  O                  E1/2  O \033[37m  |   O  \033[33m D1/2    \033[37m             || \r\n";
     char Line_18[] = "              ||           ||                         \\\\|||//                         || \r\n";
     char Line_19[] = "              ||           ||                           |||                           || \r\n";
     char Line_20[] = "              ||           ||                           |||                           || \r\n";
     char Line_21[] = "              ||           ||                         //|||\\\\                         || \r\n";
     char Line_22[] = "    \033[33m  A11/12   O     B15/16 O                  C1/2  O \033[37m  |   O  \033[33m B13/14     \033[37m          || \r\n";
     char Line_23[] = "              \\\\            \\\\                     //    |    \\\\                      || \r\n";
     char Line_24[] = "               \\\\            \\\\     \033[33m        B3/4  O             O   D15/16   \033[37m         || \r\n";
     char Line_25[] = "                \\\\            \\\\                 //              \\\\                   || \r\n";
     char Line_26[] = "       ===========\\            \\\\======O========/=====O=====O=====\\======O=====O======/|  \r\n";
     char Line_27[] = "                  \\\\            \\\\  \033[33m C9/10          B1/2  D13/14       E13/14 E9/10  \033[37m || \r\n";
     char Line_28[] = "                   \\\\	         \\\\                                                   O \033[33m D9/10 \033[37m \r\n";
     char Line_29[] = "       ==============\\            \\\\====O=====\\======O========O========/=====O=======// \r\n";
     char Line_30[] = "                     \\\\           \033[33m     C5/6 \033[37m   \\\\ \033[33m C15/16   D11/12  \033[37m  // \033[33m  E11/12 \033[37m \r\n";
     char Line_31[] = "       ===============\\===========O=============\\====================/================ \r\n";
     char Line_32[] = "                                \033[33m C7/8                                         \033[37m   \r\n";
     char Line_33[] = "\r\n";
     char Line_34[] = "\r\n";
     char Line_35[] = " 54 Triggered \r\n";
     char Line_36[] = " 77 Triggered \r\n";
     char Line_37[] = " \r\n";
     char Line_38[] = "Enter Goal Senor\r\n";
     char Line_39[] = "G \r\n";
     uart_puts(CONSOLE, "\033[2J");
     uart_puts(CONSOLE,"\033[H");
     uart_puts(CONSOLE, Line_1);
     uart_puts(CONSOLE, Line_2);
     uart_puts(CONSOLE, Line_3);
     uart_puts(CONSOLE, Line_4);
     uart_puts(CONSOLE, Line_5);
     uart_puts(CONSOLE, Line_6);
     uart_puts(CONSOLE, Line_7);
     uart_puts(CONSOLE, Line_8);
     uart_puts(CONSOLE, Line_9);
     uart_puts(CONSOLE, Line_10);
     uart_puts(CONSOLE, Line_11);
     uart_puts(CONSOLE, Line_12);
     uart_puts(CONSOLE, Line_13);
     uart_puts(CONSOLE, Line_14);
     uart_puts(CONSOLE, Line_15);
     uart_puts(CONSOLE, Line_16);
     uart_puts(CONSOLE, Line_17);
     uart_puts(CONSOLE, Line_18);
     uart_puts(CONSOLE, Line_19);
     uart_puts(CONSOLE, Line_20);
     uart_puts(CONSOLE, Line_21);
     uart_puts(CONSOLE, Line_22);
     uart_puts(CONSOLE, Line_23);
     uart_puts(CONSOLE, Line_24);
     uart_puts(CONSOLE, Line_25);
     uart_puts(CONSOLE, Line_26);
     uart_puts(CONSOLE, Line_27);
     uart_puts(CONSOLE, Line_28);
     uart_puts(CONSOLE, Line_29);
     uart_puts(CONSOLE, Line_30);
     uart_puts(CONSOLE, Line_31);
     uart_puts(CONSOLE, Line_32);
     uart_puts(CONSOLE, Line_33);
     uart_puts(CONSOLE, Line_34);
     uart_puts(CONSOLE, Line_35);
     uart_puts(CONSOLE, Line_36);
     uart_puts(CONSOLE, Line_37);
     uart_puts(CONSOLE, Line_38);
     uart_puts(CONSOLE, Line_39);
     //Console_printf(console_server_id, "Goal for Train %d is sensor is %s \r\n", Known_Trains[train_idx].id, Destination);
     //Console_printf(console_server_id, "Goal for Train %d is sensor is %s \r\n", Known_Trains[train_idx].id, Destination);
     //Console_printf(console_server_id, "Goal for Train %d is sensor is %s \r\n", Known_Trains[train_idx].id, Destination);

}

void set_A01(enum sensor_state  state) {
     //line 9, column 14
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[9;14H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[9;14H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[9;14H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[9;14H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_A03(enum sensor_state  state) {
     //line 17, column 29
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[17;29H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[17;29H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[17;29H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[17;29H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_A05(enum sensor_state  state) {
     //line 31, column 16
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[31;16H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[31;16H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[31;16H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[31;16H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_A07(enum sensor_state  state) {
     //line 29, column 15
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[29;15H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[29;15H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[29;15H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[29;15H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_A09(enum sensor_state  state) {
     //line 26, column 15
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[26;15H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[26;15H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[26;15H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[26;15H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_A11(enum sensor_state  state) {
     //line 22, column 16
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[22;16H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[22;16H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[22;16H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[22;16H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_A13(enum sensor_state  state) {
     //line 11, column 14
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[11;14H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[11;14H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[11;14H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[11;14H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_A15(enum sensor_state state) {
     //line 17, column 16
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[17;16H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[17;16H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[17;16H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[17;16H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_all_A(int console_server_id, uint8_t state){
     //set_A01(unvisited);
     set_A03(state);
     //set_A05(cpu_train);
     //set_A07(both_train);
     //set_A09(state);
     set_A11(state);
     //set_A13(state);
     set_A15(state);
}

void set_B01(enum sensor_state  state) {
     //line 26, column 55
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[26;55H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[26;55H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[26;55H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[26;55H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_B03(enum sensor_state  state) {
     //line 24, column 51
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[24;51H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[24;51H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[24;51H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[24;51H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_B05(enum sensor_state  state) {
     //line 13, column 54
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[13;54H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[13;54H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[13;54H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[13;54H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_B07(enum sensor_state  state) {
     //NOT USED
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[29;15H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[29;15H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[29;15H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[29;15H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_B09(enum sensor_state  state) {
     //NOT USED
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[26;15H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[26;15H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[26;15H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[26;15H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_B11(enum sensor_state  state) {
     //NOT USED
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[22;15H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[22;15H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[22;15H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[22;15H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_B13(enum sensor_state  state) {
     //line 22, column 62
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[22;62H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[22;62H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[22;62H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[22;62H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_B15(enum sensor_state state) {
     //line 22, column 29
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[22;29H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[22;29H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[22;29H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[22;29H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_all_B(int console_server_id, uint8_t state){
     set_B01(state);
     set_B03(state);
     set_B05(state);
     //set_B07(both_train);
     //set_B09(state);
     //set_B11(state);
     set_B13(state);
     set_B15(state);
}

void set_C01(enum sensor_state  state) {
     //line 22, column 54
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[22;54H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[22;54H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[22;54H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[22;54H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_C03(enum sensor_state  state) {
     //NOT USED
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[17;29H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[17;29H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[17;29H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[17;29H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_C05(enum sensor_state  state) {
     //line 29, column 41
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[29;41H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[29;41H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[29;41H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[29;41H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_C07(enum sensor_state  state) {
     //line 31, column 35
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[31;35H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[31;35H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[31;35H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[31;35H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_C09(enum sensor_state  state) {
     //line 26, column 40
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[26;40H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[26;40H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[26;40H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[26;40H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_C11(enum sensor_state  state) {
     //line 13, column 38
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[13;38H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[13;38H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[13;38H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[13;38H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_C13(enum sensor_state  state) {
     //line 9, column 52
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[9;52H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[9;52H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[9;52H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[9;52H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_C15(enum sensor_state state) {
     //line 29, column 54
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[29;54H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[29;54H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[29;54H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[29;54H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_all_C(int console_server_id, uint8_t state){
     set_C01(state);
     //set_C03(user_train);
     set_C05(state);
     set_C07(state);
     set_C09(state);
     set_C11(state);
     set_C13(state);
     set_C15(state);
}

void set_D01(enum sensor_state  state) {
     //line 17, column 62
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[17;62H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[17;62H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[17;62H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[17;62H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_D03(enum sensor_state  state) {
     //line 13, column 60
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[13;60H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[13;60H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[13;60H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[13;60H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_D05(enum sensor_state  state) {
     //line 13, column 78
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[13;78H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[13;78H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[13;78H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[13;78H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_D07(enum sensor_state  state) {
     //line 11, column 87
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[11;87H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[11;87H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[11;87H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[11;87H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_D09(enum sensor_state  state) {
     //line 28, column 87
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[28;87H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[28;87H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[28;87H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[28;87H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_D11(enum sensor_state  state) {
     //line 29, column 63
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[29;63H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[29;63H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[29;63H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[29;63H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_D13(enum sensor_state  state) {
     //line 26, column 61
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[26;61H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[26;61H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[26;61H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[26;61H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_D15(enum sensor_state state) {
     //line 24, column 65
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[24;65H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[24;65H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[24;65H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[24;65H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_all_D(int console_server_id, uint8_t state){
     set_D01(state);
     set_D03(state);
     set_D05(state);
     set_D07(state);
     set_D09(state);
     set_D11(state);
     set_D13(state);
     set_D15(state);
}

void set_E01(enum sensor_state  state) {
     //line 17, column 54
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[17;54H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[17;54H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[17;54H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[17;54H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_E03(enum sensor_state  state) {
     //line 15, column 65
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[15;65H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[15;65H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[15;65H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[15;65H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_E05(enum sensor_state  state) {
     //line 13, column 72
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[13;72H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[13;72H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[13;72H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[13;72H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_E07(enum sensor_state  state) {
     //line 9, column 69
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[9;69H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[9;69H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[9;69H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[9;69H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_E09(enum sensor_state  state) {
     //line 26, column 80
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[26;80H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[26;80H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[26;80H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[26;80H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_E11(enum sensor_state  state) {
     //line 29, column 78
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[29;78H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[29;78H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[29;78H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[29;78H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_E13(enum sensor_state  state) {
     //line 26, column 74
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[26;74H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[26;74H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[26;74H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[26;74H\033[34mO\033[37m\033[40;1H");
               break;
     }

}

void set_E15(enum sensor_state state) {
     //line 15, column 51
     switch(state){
          case unvisited:
               //yellow
               uart_puts(CONSOLE, "\033[15;51H\033[33mO\033[37m\033[40;1H");
               break;
          case user_train:
               //cyan
               uart_puts(CONSOLE, "\033[15;51H\033[36mO\033[37m\033[40;1H");
               break;
          case cpu_train:
               //magenta
               uart_puts(CONSOLE, "\033[15;51H\033[35mO\033[37m\033[40;1H");
               break;
          case both_train:
               //blue
               uart_puts(CONSOLE, "\033[15;51H\033[34mO\033[37m\033[40;1H");
               break;
     }
}

void set_all_E(int console_server_id, uint8_t state){
     set_E01(state);
     set_E03(state);
     set_E05(state);
     set_E07(state);
     set_E09(state);
     set_E11(state);
     set_E13(state);
     set_E15(state);
}

void Update_sensors_GUI(int sensor, int train, char user_train_sensors[NUM_GOAL_SENSORS], char cpu_train_sensors[NUM_GOAL_SENSORS]){
     //Sensor set A
     if ((sensor == 2)|(sensor == 3)){ // A03/A04
          if (train == USER_TRAIN) {
               user_train_sensors[2] = 1;
               user_train_sensors[3] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[2] = 1;
               cpu_train_sensors[3] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_A03(both_train);
          else if (user_train_sensors[sensor])
               set_A03(user_train);
          else if (cpu_train_sensors[sensor])
               set_A03(cpu_train);
     }
     else if ((sensor == 10)|(sensor == 11)){ // A11/A12
          if (train == USER_TRAIN) {
               user_train_sensors[10] = 1;
               user_train_sensors[11] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[10] = 1;
               cpu_train_sensors[11] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_A11(both_train);
          else if (user_train_sensors[sensor])
               set_A11(user_train);
          else if (cpu_train_sensors[sensor])
               set_A11(cpu_train);
     }
     else if ((sensor == 14)|(sensor == 15)){ // A15/A16
          if (train == USER_TRAIN) {
               user_train_sensors[14] = 1;
               user_train_sensors[15] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[14] = 1;
               cpu_train_sensors[15] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_A15(both_train);
          else if (user_train_sensors[sensor])
               set_A15(user_train);
          else if (cpu_train_sensors[sensor])
               set_A15(cpu_train);
     }
     //Sensor set B
     else if ((sensor == 16)|(sensor == 17)){ // B01/B02
          if (train == USER_TRAIN) {
               user_train_sensors[16] = 1;
               user_train_sensors[17] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[16] = 1;
               cpu_train_sensors[17] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_B01(both_train);
          else if (user_train_sensors[sensor])
               set_B01(user_train);
          else if (cpu_train_sensors[sensor])
               set_B01(cpu_train);
     }
     else if ((sensor == 18)|(sensor == 19)){ // B03/B04
          if (train == USER_TRAIN) {
               user_train_sensors[18] = 1;
               user_train_sensors[19] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[18] = 1;
               cpu_train_sensors[19] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_B03(both_train);
          else if (user_train_sensors[sensor])
               set_B03(user_train);
          else if (cpu_train_sensors[sensor])
               set_B03(cpu_train);
     }
     else if ((sensor == 20)|(sensor == 21)){ // B05/B06
          if (train == USER_TRAIN) {
               user_train_sensors[20] = 1;
               user_train_sensors[21] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[20] = 1;
               cpu_train_sensors[21] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_B05(both_train);
          else if (user_train_sensors[sensor])
               set_B05(user_train);
          else if (cpu_train_sensors[sensor])
               set_B05(cpu_train);
     }
     else if ((sensor == 28)|(sensor == 29)){ // B13/B14
          if (train == USER_TRAIN) {
               user_train_sensors[28] = 1;
               user_train_sensors[29] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[28] = 1;
               cpu_train_sensors[29] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_B13(both_train);
          else if (user_train_sensors[sensor])
               set_B13(user_train);
          else if (cpu_train_sensors[sensor])
               set_B13(cpu_train);
     }
     else if ((sensor == 30)|(sensor == 31)){ // B15/B16
          if (train == USER_TRAIN) {
               user_train_sensors[30] = 1;
               user_train_sensors[31] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[30] = 1;
               cpu_train_sensors[31] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_B15(both_train);
          else if (user_train_sensors[sensor])
               set_B15(user_train);
          else if (cpu_train_sensors[sensor])
               set_B15(cpu_train);
     }
     //Sensor set C (1<<0 to 1<<7)
     else if ((sensor == 32)|(sensor == 33)){ // C01/C02
          if (train == USER_TRAIN) {
               user_train_sensors[32] = 1;
               user_train_sensors[33] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[32] = 1;
               cpu_train_sensors[33] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_C01(both_train);
          else if (user_train_sensors[sensor])
               set_C01(user_train);
          else if (cpu_train_sensors[sensor])
               set_C01(cpu_train);
     }
     else if ((sensor == 36)|(sensor == 37)){ // C05/C06
          if (train == USER_TRAIN) {
               user_train_sensors[36] = 1;
               user_train_sensors[37] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[36] = 1;
               cpu_train_sensors[37] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_C05(both_train);
          else if (user_train_sensors[sensor])
               set_C05(user_train);
          else if (cpu_train_sensors[sensor])
               set_C05(cpu_train);
     }
     else if ((sensor == 38)|(sensor == 39)){ // C07/C08
          if (train == USER_TRAIN) {
               user_train_sensors[38] = 1;
               user_train_sensors[39] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[38] = 1;
               cpu_train_sensors[39] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_C07(both_train);
          else if (user_train_sensors[sensor])
               set_C07(user_train);
          else if (cpu_train_sensors[sensor])
               set_C07(cpu_train);
     }
     else if ((sensor == 40)|(sensor == 41)){ // C09/C10
          if (train == USER_TRAIN) {
               user_train_sensors[40] = 1;
               user_train_sensors[41] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[40] = 1;
               cpu_train_sensors[41] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_C09(both_train);
          else if (user_train_sensors[sensor])
               set_C09(user_train);
          else if (cpu_train_sensors[sensor])
               set_C09(cpu_train);
     }
     else if ((sensor == 42)|(sensor == 43)){ // C11/C12
          if (train == USER_TRAIN) {
               user_train_sensors[42] = 1;
               user_train_sensors[43] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[42] = 1;
               cpu_train_sensors[43] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_C11(both_train);
          else if (user_train_sensors[sensor])
               set_C11(user_train);
          else if (cpu_train_sensors[sensor])
               set_C11(cpu_train);
     }
     else if ((sensor == 44)|(sensor == 45)){ // C13/C14
          if (train == USER_TRAIN) {
               user_train_sensors[44] = 1;
               user_train_sensors[45] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[44] = 1;
               cpu_train_sensors[45] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_C13(both_train);
          else if (user_train_sensors[sensor])
               set_C13(user_train);
          else if (cpu_train_sensors[sensor])
               set_C13(cpu_train);
     }
     else if ((sensor == 46)|(sensor == 47)){ // C15/C16
          if (train == USER_TRAIN) {
               user_train_sensors[46] = 1;
               user_train_sensors[47] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[46] = 1;
               cpu_train_sensors[47] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_C15(both_train);
          else if (user_train_sensors[sensor])
               set_C15(user_train);
          else if (cpu_train_sensors[sensor])
               set_C15(cpu_train);
     }
     //Sensor set D (1<<0 to 1<<7)
     else if ((sensor == 48)|(sensor == 49)){ // D01/D02
          if (train == USER_TRAIN) {
               user_train_sensors[48] = 1;
               user_train_sensors[49] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[48] = 1;
               cpu_train_sensors[49] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_D01(both_train);
          else if (user_train_sensors[sensor])
               set_D01(user_train);
          else if (cpu_train_sensors[sensor])
               set_D01(cpu_train);
     }
     else if ((sensor == 50)|(sensor == 51)){ // D03/D04
          if (train == USER_TRAIN) {
               user_train_sensors[50] = 1;
               user_train_sensors[51] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[50] = 1;
               cpu_train_sensors[51] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_D03(both_train);
          else if (user_train_sensors[sensor])
               set_D03(user_train);
          else if (cpu_train_sensors[sensor])
               set_D03(cpu_train);
     }
     else if ((sensor == 52)|(sensor == 53)){ // D05/D06
          if (train == USER_TRAIN) {
               user_train_sensors[52] = 1;
               user_train_sensors[53] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[52] = 1;
               cpu_train_sensors[53] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_D05(both_train);
          else if (user_train_sensors[sensor])
               set_D05(user_train);
          else if (cpu_train_sensors[sensor])
               set_D05(cpu_train);
     }
     else if ((sensor == 54)|(sensor == 55)){ // D07/D08
          if (train == USER_TRAIN) {
               user_train_sensors[54] = 1;
               user_train_sensors[55] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[54] = 1;
               cpu_train_sensors[55] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_D07(both_train);
          else if (user_train_sensors[sensor])
               set_D07(user_train);
          else if (cpu_train_sensors[sensor])
               set_D07(cpu_train);
     }
     else if ((sensor == 56)|(sensor == 57)){ // D09/D10
          if (train == USER_TRAIN) {
               user_train_sensors[56] = 1;
               user_train_sensors[57] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[56] = 1;
               cpu_train_sensors[57] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_D09(both_train);
          else if (user_train_sensors[sensor])
               set_D09(user_train);
          else if (cpu_train_sensors[sensor])
               set_D09(cpu_train);
     }
     else if ((sensor == 58)|(sensor == 59)){ // D11/D12
          if (train == USER_TRAIN) {
               user_train_sensors[58] = 1;
               user_train_sensors[59] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[58] = 1;
               cpu_train_sensors[59] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_D11(both_train);
          else if (user_train_sensors[sensor])
               set_D11(user_train);
          else if (cpu_train_sensors[sensor])
               set_D11(cpu_train);
     }
     else if ((sensor == 60)|(sensor == 61)){ // D13/D14
          if (train == USER_TRAIN) {
               user_train_sensors[60] = 1;
               user_train_sensors[61] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[60] = 1;
               cpu_train_sensors[61] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_D13(both_train);
          else if (user_train_sensors[sensor])
               set_D13(user_train);
          else if (cpu_train_sensors[sensor])
               set_D13(cpu_train);
     }
     else if ((sensor == 62)|(sensor == 63)){ // D15/D16
          if (train == USER_TRAIN) {
               user_train_sensors[62] = 1;
               user_train_sensors[63] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[62] = 1;
               cpu_train_sensors[63] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_D15(both_train);
          else if (user_train_sensors[sensor])
               set_D15(user_train);
          else if (cpu_train_sensors[sensor])
               set_D15(cpu_train);
     }
     //Sensor set E (1<<0 to 1<<7)
     else if ((sensor == 64)|(sensor == 65)){ // E01/E02
          if (train == USER_TRAIN) {
               user_train_sensors[64] = 1;
               user_train_sensors[65] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[64] = 1;
               cpu_train_sensors[65] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_E01(both_train);
          else if (user_train_sensors[sensor])
               set_E01(user_train);
          else if (cpu_train_sensors[sensor])
               set_E01(cpu_train);
     }
     else if ((sensor == 66)|(sensor == 67)){ // E03/E04
          if (train == USER_TRAIN) {
               user_train_sensors[66] = 1;
               user_train_sensors[67] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[66] = 1;
               cpu_train_sensors[67] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_E03(both_train);
          else if (user_train_sensors[sensor])
               set_E03(user_train);
          else if (cpu_train_sensors[sensor])
               set_E03(cpu_train);
     }
     else if ((sensor == 68)|(sensor == 69)){ // E05/E06
          if (train == USER_TRAIN) {
               user_train_sensors[68] = 1;
               user_train_sensors[69] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[68] = 1;
               cpu_train_sensors[69] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_E05(both_train);
          else if (user_train_sensors[sensor])
               set_E05(user_train);
          else if (cpu_train_sensors[sensor])
               set_E05(cpu_train);
     }
     else if ((sensor == 70)|(sensor == 71)){ // E07/E08
          if (train == USER_TRAIN) {
               user_train_sensors[70] = 1;
               user_train_sensors[71] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[70] = 1;
               cpu_train_sensors[71] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_E07(both_train);
          else if (user_train_sensors[sensor])
               set_E07(user_train);
          else if (cpu_train_sensors[sensor])
               set_E07(cpu_train);
     }
     else if ((sensor == 72)|(sensor == 73)){ // E09/E10
          if (train == USER_TRAIN) {
               user_train_sensors[72] = 1;
               user_train_sensors[73] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[72] = 1;
               cpu_train_sensors[73] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_E09(both_train);
          else if (user_train_sensors[sensor])
               set_E09(user_train);
          else if (cpu_train_sensors[sensor])
               set_E09(cpu_train);
     }
     else if ((sensor == 74)|(sensor == 75)){ // E11/E12
          if (train == USER_TRAIN) {
               user_train_sensors[74] = 1;
               user_train_sensors[75] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[74] = 1;
               cpu_train_sensors[75] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_E11(both_train);
          else if (user_train_sensors[sensor])
               set_E11(user_train);
          else if (cpu_train_sensors[sensor])
               set_E11(cpu_train);
     }
     else if ((sensor == 76)|(sensor == 77)){ // E13/E14
          if (train == USER_TRAIN) {
               user_train_sensors[76] = 1;
               user_train_sensors[77] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[76] = 1;
               cpu_train_sensors[77] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_E13(both_train);
          else if (user_train_sensors[sensor])
               set_E13(user_train);
          else if (cpu_train_sensors[sensor])
               set_E13(cpu_train);
     }
     else if ((sensor == 78)|(sensor == 79)){ // E15/E16
          if (train == USER_TRAIN) {
               user_train_sensors[78] = 1;
               user_train_sensors[79] = 1;
          }
          else if (train == CPU_TRAIN) {
               cpu_train_sensors[78] = 1;
               cpu_train_sensors[79] = 1;
          }
          
          if (user_train_sensors[sensor] && cpu_train_sensors[sensor])
               set_E15(both_train);
          else if (user_train_sensors[sensor])
               set_E15(user_train);
          else if (cpu_train_sensors[sensor])
               set_E15(cpu_train);
     }

}


void set_Dummy_sensors(int console_server_id, uint8_t state){
//User: A11, A15, C13, E8, A3, B15
     set_A11(user_train);
     set_A15(user_train);
     set_C13(user_train);
     set_E07(user_train);

     //Computer
     set_C07(cpu_train);
     set_E11(cpu_train);
     set_D09(cpu_train);
     set_D05(cpu_train);
     set_E05(cpu_train);
     set_E03(cpu_train);
     set_D01(cpu_train);
     set_C01(cpu_train);
     set_B03(cpu_train);
     set_C09(cpu_train);
     set_C11(cpu_train);
     set_E15(cpu_train);

     //Both
     set_A03(both_train);
     set_B15(both_train);
}

void set_Dummy_messages(int console_server_id){
     uart_printf(CONSOLE, "\033[36;1HUser Most Recent Sensor B16");
     uart_printf(CONSOLE, "\033[35;1HComputer Most Recent Sensor E16");
     Console_printf(console_server_id, "\033[39;1HB16\033[85;1H");
     Console_printf(console_server_id, "\033[38;1HEnter Goal Sensor for User Train (77) (i.e. A02, E13 etc.):\033[85;1H");
}