#include <stdio.h>
#include "bitio.h"

/* En realidad 2**bit_to_read, donde q bit_to_read va desde 0 hasta
   7. */
static int bit_to_read = 256; 
static int input_byte = 0;
static int bit_to_write = 1;
static int output_byte = 0;
static int get_counter = 0;
static int put_counter = 0;

#define FEEDBACK 8192

int get_bit() {
  int bit;
  if(bit_to_read==256) {
    input_byte = getchar();
    bit_to_read = 1;
  }
  bit = input_byte & bit_to_read;
  bit_to_read <<= 1;
  get_counter++;
  if(!(get_counter%8192)) fprintf(stderr,".");
  return bit;
}  

int get_bits(int number_of_bits_to_get) {
  int i;
  int s = (get_bit() ? 1:0);
  for(i=1; i<number_of_bits_to_get; i++) {
    s <<= 1;
    s |= (get_bit() ? 1:0);
  }
  return s;
}

void put_bit(int bit) {
  if(bit_to_write==256) {
    putchar(output_byte);
    bit_to_write = 1;
    output_byte = 0;
  }
  if(bit) output_byte |= bit_to_write;
  bit_to_write <<= 1;
  put_counter++;
  if(!(put_counter%8192)) fprintf(stderr,"o");
}

void put_bits(int bits, int number_of_bits_to_put) {
  int i;
  for(i=number_of_bits_to_put-1; i>=0; i--) {
    put_bit(bits & (1<<i));
  }
}

void flush() {
  if(bit_to_write!=0) putchar(output_byte);
}
