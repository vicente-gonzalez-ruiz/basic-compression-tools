/*
 * rle.c
 *
 * MNP-5 RLE Codec.
 *
 * Encodes a sequence of symbols using the paterns:
 *
 * input output
 * ----- ------
 * ab    ab
 * aab   aa0b
 * aaab  aa1b
 * aaaab aa2b
 *
 * Typical use:
 *
 * rle < raw-file > compressed-file
 * rle < raw-file | bwt | mtf | rle | ari > compressed-file
 *
 * References:
 *
 * M. Nelson and J.-L. Gailly, The Data Compression Book. 1995.
 */

#include <stdio.h>

void read_and_encode_run(int *symbol, int prev_symbol) {
  if(*symbol == prev_symbol) {
    int length = 0;
    *symbol = getchar();
    
    while((*symbol != EOF) && (length < 255)) {
      if(*symbol == prev_symbol) {
	*symbol = getchar();
	length++;
      }
      else break;
    }
    putchar(length);
    if((length != 255) && (*symbol != EOF)) {
      putchar(*symbol);
    }
  }
}

void encode_stream() {
  int prev_symbol = 0;
  int symbol = getchar();
  while (symbol != EOF) {
    putchar(symbol);
    read_and_encode_run(&symbol, prev_symbol);
    prev_symbol = symbol;
    symbol = getchar();
  }
}

void read_and_decode_run(int symbol, int prev_symbol) {
  if(symbol == prev_symbol) {
    int length = getchar();
    while(length-- > 0) {
      putchar(symbol);
    }
  }
}

void decode_stream() {
  int prev_symbol = 0;
  int symbol = getchar();;
  while(symbol != EOF)  {
    putchar(symbol);
    read_and_decode_run(symbol, prev_symbol);
    prev_symbol = symbol;
    symbol = getchar();
  }
}
