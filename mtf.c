/*
 * mtf.c
 *
 * Move-To-Front encoding.
 *
 * Referencias:
 *
 * "A Locally Adaptive Data Compression Scheme" by J. L. Bentley,
 * D. D. Sleator, R. E. Tarjan, V. K. Wei, Communications of the
 * ACM-Vol. 29, No. 4, 1986
 */

#include <stdio.h>
#include "codec.h"

unsigned char order[ 256 ];

void encode_stream(int argc, char *argv[]) {
  int i, c, j;
  for ( i = 0 ; i < 256 ; i++ )
    order[ i ] = (unsigned char) i;
  while ( ( c = getchar() ) >= 0 )  {
    //
    // Find the char, and output it
    //
    for ( i = 0 ; i < 256 ; i++ )
      if ( order[ i ] == ( c & 0xff ) )
	break;
    putchar( (char) i);
    //
    // Now shuffle the order array
    //
    for ( j = i ; j > 0 ; j-- )
      order[ j ] = order[ j - 1 ];
    order[ 0 ] = (unsigned char) c;
  }
}

void decode_stream(int argc, char *argv[]) {
  int i, j, c;
  for ( i = 0 ; i < 256 ; i++ )
    order[ i ] = (unsigned char) i;
  while ( ( i = getchar() ) >= 0 )  {
    //
    // Find the char
    //
    putchar( order[ i ] );
    c = order[ i ];
    //
    // Now shuffle the order array
    //
    for ( j = i ; j > 0 ; j-- )
      order[ j ] = order[ j - 1 ];
    order[ 0 ] = (unsigned char) c;
  }
}
