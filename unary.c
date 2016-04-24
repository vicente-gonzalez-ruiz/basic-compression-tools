/*
 * unary.c
 *
 * Un codificador unario.
 *
 * Referencias:
 *
 * Khalid Sayood, Data Compression, 3rd ed, Morgan Kaufmann.
 * K. R. Rao, Principles of Digital Video Coding.
 */

#include <stdio.h>
#include "bitio.h"
#include "vlc.h"

/* Inicializa el codificador. */
void init_encoder() {
}

/* Inicializa el descodificador. */
void init_decoder() {
}

/* Codifica el índice "index" usando el espacio de recuentos
   acumulados "cum_freq". */
void encode_index(int index, unsigned short *cum_prob) {
  int i, s;
  s = index - 1;
  for(i=0; i<s; i++) {
    put_bit(1);
  }
  put_bit(0);
}

/* Descodifica el siguiente índice . */
int decode_index(unsigned short *cum_prob) {
  int s = 0;
  while(get_bit()) {
    s++;
  }
  return s+1;
}

/* Finaliza el codificador. */
void finish_encoder() {
  flush();
}

/* Finaliza el descodificador. */
void finish_decoder() {
}
