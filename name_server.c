#include "name_server.h"
#include "tasks.h"
#include "rpi.h"
#include "util.h"
#include "kernel.h" //For NAME_SERVER_TID

typedef struct Name_storage {
    char name [MAX_NAME_LENGTH];
    int id; //2 digits + null terminator
} Name_storage;

void Name_server_task() {
    int myid = MyTid(); 
    if (myid != NAME_SERVER_TID) {
        uart_puts(CONSOLE,"\033[32m");
        uart_printf(CONSOLE, "Name server did not get proper id");
        uart_puts(CONSOLE,"\033[37m"); 
        Exit();
    }
    Name_storage name_book[MAX_NAMES] = {0};

    //set to default values
    int next_name_slot = 0;
    char temp_buffer[MAX_NAME_LENGTH + 1]; //1 extra character to encode the request type
    //reset temp_buffer
    memset(temp_buffer, 0, MAX_NAME_LENGTH + 2);

    char Register_Reply[4] = "ack";
    char Who_Reply[3] = {'\0','n','\0'}; // largest possible tid is 50, just need one char
    char Invalid_Reply[4] = "bad";

    for(;;) {
        int sender_id = 0;
        int msg_len = 0;

        msg_len = Receive(&sender_id, temp_buffer, MAX_NAME_LENGTH + 1);

        switch ( temp_buffer[0]) {
            case 'R':   //Register Case
                if (next_name_slot < MAX_NAMES) {
                    //uart_printf(CONSOLE, "Name Server: Received register request for name: %s \r\n", temp_buffer+1);
                    
                    int found = 0;
                    for(int i = 0; i < next_name_slot; i++){
                        int compare = cust_strcmp(temp_buffer + 1, name_book[i].name);
                        if (compare == 0) {
                            uart_printf(CONSOLE, "Duplicate name '%s' - Previously associated with tid: %d. Replacing with new tid: %d \r\n", temp_buffer + 1, name_book[i].id, sender_id);
                            name_book[i].id = sender_id;
                            found = 1;
                        }
                    }
                    if (found == 0) { //Non duplicate name
                        memcpy(name_book[next_name_slot].name, temp_buffer+1 , msg_len-1); //strip out the request type info
                        name_book[next_name_slot].id = sender_id;
                        //uart_printf(CONSOLE, "Name Server: Registered with id %d \r\n", name_book[next_name_slot].id);
                        next_name_slot++;
                    }
                    Reply(sender_id, Register_Reply, 4);
                }
                break;
            case 'W':   //Who am I case
                int found = 0;
                //uart_printf(CONSOLE, "Received Who is request for name: %s \r\n", temp_buffer+1);
                for(int i = 0; i < next_name_slot; i++){
                    int compare = cust_strcmp(temp_buffer + 1, name_book[i].name);
                    if (compare == 0) {
                        Who_Reply[0] = name_book[i].id;
                        Who_Reply[1] = 'f';
                        Reply(sender_id, Who_Reply, 3);
                        found = 1;
                        break;
                    }
                }
                if (found != 1) {
                    Who_Reply[0] = 0;
                    Who_Reply[1] = 'n';
                    Reply(sender_id, Who_Reply, 3);
                }
                //assume always found
                break;
            default:    //Invalid Case 
                Reply(sender_id, Invalid_Reply, 4);
                break;
        }
        //reset temp_buffer
        memset(temp_buffer, 0, MAX_NAME_LENGTH + 2);

    }
    Exit();
}


