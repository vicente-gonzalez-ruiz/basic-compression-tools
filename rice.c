/*
 * unary.c
 *
 * Un codificador de Rice.
 *
 * Referencias:
 *
 * R. F. Rice, "Some Practical Universal Noiseless Coding Techniques,
 * " Jet Propulsion Laboratory, Pasadena, California, JPL Publication
 * 79--22, Mar. 1979.
 *
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

/* Calcula el recuento del símbolo x. */
static int prob(unsigned short *cum_prob, int x) {
  return cum_prob[x-1] - cum_prob[x];
}
/* Estima la pendiente de la distribución de probabilidades de los
   símbolos. Se presupone que existen 256 símbolos en el alfabeto. */
int estimate_k(unsigned short *cum_prob) {
  int k = 0;
  int i = 1;
  while(prob(cum_prob, i+1) > prob(cum_prob, i)/2) {
    /* Si la probabilidad del símbolo "i+1" es mayor o igual que la
       mitad del símbolo "i", dejamos de incrementar la "k". */
    i++;
    k++;
    if(k>7) break;
  }
  /* Debido a una limitación de "bitio", no podemos generar códigos
     unarios más largos de 32 bits (256/32=8=2^3). */
  if(k<3) k=3; 
  return k;
}

/* Codifica el índice "index" usando el espacio de recuentos
   acumulados "cum_freq". */
void encode_index(int index, unsigned short *cum_prob) {
  int i, k, m, s;
  k = estimate_k(cum_prob);
  m = 1<<k;
  s = index - 1;
  for(i=0; i<(s/m); i++) {
    put_bit(1);
  }
  put_bit(0);
  put_bits(s, k);
}

/* Descodifica el siguiente índice. */
int decode_index(unsigned short *cum_prob) {
  int i, x = 0, s, k;
  k = estimate_k(cum_prob);
  s = 0;
  while(get_bit()) {
    s++;
  }
  if (k) {
    x = get_bits(k);
    s = (s<<k) + x;
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
