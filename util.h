#ifndef _util_h_
#define _util_h_ 1

#include <stddef.h>

// conversions
int a2d(char ch);
char a2i( char ch, char **src, int base, int *nump );
void ui2a( unsigned int num, unsigned int base, char *bf );
void i2a( int num, char *bf );

// memory
void *memset(void *s, int c, size_t n);
void* memcpy(void* restrict dest, const void* restrict src, size_t n);

// String functions
int cust_strlen(const char * mystr);
int cust_strcmp(const char *true_ver, const char *candidate);

//message int conversion
void int2char_arr(int my_int, char *char_array, int start_index);
int char_arr2int(char *char_array, int start_index);

#endif /* util.h */
