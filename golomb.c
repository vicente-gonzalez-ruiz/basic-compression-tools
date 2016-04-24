/*
 * unary.c
 *
 * Un codificador unario.
 *
 * Referencias:
 *
 * Golomb, S.W. (1966). , Run-length encodings. IEEE Transactions on
 * Information Theory, IT--12(3):399--401.
 *
 * Witten, Ian Moffat, Alistair Bell, Timothy. "Managing Gigabytes:
 * Compressing and Indexing Documents and Images." Second
 * Edition. Morgan Kaufmann Publishers, San Francisco CA. 1999 ISBN
 * 1-55860-570-3.
 *
 * David Salomon. "Data Compression",ISBN 0-387-95045-1.
 */

#include <stdio.h>
#include <math.h>
#include "bitio.h"
#include "vlc.h"

/* Inicializa el codificador. */
void init_encoder() {
}

/* Inicializa el descodificador. */
void init_decoder() {
}

/* Calcula el recuento del símbolo x. */
static int prob(unsigned short *cum_prob, int x) {
  return cum_prob[x-1] - cum_prob[x];
}
/* Estima la pendiente de la distribución de probabilidades de los
   símbolos. Se presupone que existen 256 símbolos en el alfabeto. */
int estimate_m(unsigned short *cum_prob) {
  int m;
  m = 255-(255.0*(cum_prob[0] - cum_prob[1]))/cum_prob[0];
  /* Debido a una limitación de "bitio", no podemos generar códigos
     unarios más largos de 32 bits (256/32=8). */
  if(m<8) m=8; 
  return m;
}

/* Codifica el índice "index" usando el espacio de recuentos
   acumulados "cum_freq". */
void encode_index(int index, unsigned short *cum_prob) {
  int i, k, m, s ,t, r;
  m = estimate_m(cum_prob);
  k = ceil(log((float)m)/log(2.0));
  t = (1<<k)-m;
  s = index - 1;
  r = s % m;
  for(i=0; i<(s/m); i++) {
    put_bit(1);
  }
  put_bit(0);
  if(r<t) {
    put_bits(r, k-1);
  } else {
    r += t;
    put_bits(r, k);
  }
}

static int next_bit() {
  if(get_bit()) return 1;
  return 0;
}

/* Descodifica el siguiente índice. */
int decode_index(unsigned short *cum_prob) {
  int i, x, s, k, m, t;
  m = estimate_m(cum_prob);
  k = ceil(log((float)m)/log(2.0));
  t = (1<<k)-m;
  s = 0;
  while(get_bit()) {
    s++;
  }
  x = get_bits(k-1);
  if (x<t) {
    s = s*m + x;
  } else {
    x = x*2 + next_bit();
    s = s*m + x - t;
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
