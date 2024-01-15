#include "util.h"
#include "rpi.h"

// ascii digit to integer
int a2d( char ch ) {
	if( ch >= '0' && ch <= '9' ) return ch - '0';
	if( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

// unsigned int to ascii string
void ui2a( unsigned int num, unsigned int base, char *bf ) {
	int n = 0;
	int dgt;
	unsigned int d = 1;

	while( (num / d) >= base ) d *= base;
	while( d != 0 ) {
		dgt = num / d;
		num %= d;
		d /= base;
		if( n || dgt > 0 || d == 0 ) {
			*bf++ = dgt + ( dgt < 10 ? '0' : 'a' - 10 );
			++n;
		}
	}
	*bf = 0;
}

// signed int to ascii string
void i2a( int num, char *bf ) {
	if( num < 0 ) {
		num = -num;
		*bf++ = '-';
	}
	ui2a( num, 10, bf );
}

// define our own memset to avoid SIMD instructions emitted from the compiler
void *memset(void *s, int c, size_t n) {
  for (char* it = (char*)s; n > 0; --n) *it++ = c;
  return s;
}

// define our own memcpy to avoid SIMD instructions emitted from the compiler
void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    char* sit = (char*)src;
    char* cdest = (char*)dest;
    for (size_t i = 0; i < n; ++i) *(cdest++) = *(sit++);
    return dest;
}

//count String length
int cust_strlen(const char * mystr) {
    int length = 0;
    while( mystr[length] != '\0') {
        length++;
    }
    length++; //include null terminator in the count
    return length;
}

//string compare
//returns 0 on sucess to match standard but tbh the standard is a little silly
int cust_strcmp(const char *true_ver, const char *candidate) {
    //uart_printf(1, "first is %s \r\n", true_ver);
	//uart_printf(1, "second is %s \r\n", candidate);

	if (true_ver[0] == '\0')
        return -2; //Inform user they are comparing against an empty string
    int length = 0;
    while(true_ver[length] != '\0') {
        if (true_ver[length] != candidate[length]){
            return -1;
        }
        length++;
    }
    if (candidate[length] != '\0'){ //Make sure they end the same
        return -1;
    }
	//uart_printf(1, "MATCH!! \r\n");
    return 0;
}

void int2char_arr(int my_int, char *char_array, int start_index) {
    int mod_ticks = my_int;
    char cur_char = 0;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    char_array[start_index+3] = cur_char;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    char_array[start_index+2] = cur_char;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    char_array[start_index+1] = cur_char;
    cur_char = mod_ticks%255;
    mod_ticks = mod_ticks/255;
    char_array[start_index]= cur_char;
	/*
    void *myptr;
    myptr = char_array + start_index;
    int * intptr = myptr;
    *intptr = my_int;
	*/
    //printf("expectd is %x \n", my_int);
    //printf("ptr is is %x \n", *intptr);
}

int char_arr2int(char *char_array, int start_index) {
	//uart_printf(CONSOLE, "In char arr 2 int \r\n");
	int my_int = char_array[start_index]*(255^3) + char_array[start_index+1]*(255^2) + char_array[start_index+2]*(255) + char_array[start_index+3];
	/*
    void *myptr;
    myptr = char_array + start_index;
    int * intptr = myptr;
	*/
    return my_int;
}
