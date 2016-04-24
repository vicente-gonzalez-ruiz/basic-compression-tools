/*
 * model_a0.c
 *
 * Un modelo probabilístico de orden 0.
 *
 * Referencias:
 *
 * Witten, Neal, and Cleary, CACM, 1987.
 *
 * M. Nelson and J.-L. Gailly, The Data Compression Book. 1995.
 */

#include <stdio.h>
#include "vlc.h"
#include "codec.h"

/* Tamaño del alfabeto fuente. Los 256 caracteres posibles y el
   símbolo EOS (End Of Stream). */
#define ALPHA_SIZE 257

/* model_0.h */

/* Código de compresión que indica el fin del stream de datos. Su
   posición dentro del alfabeto fuente será siempre al final del
   mismo. */
#define EOS (ALPHA_SIZE-1)

/* Máximo recuento acumulado permitido. Este valor afecta a la
   precisión del modelo probabilístico a la hora de calcular las
   probabilidades de los símbolos. */
#define MAX_CUM_COUNT 16383

/* Probabilidad (en forma de recuento) de los índices. Cada índice
   está asociado a un símbolo diferente, cumpliéndose que el índice 0
   no se pude usar para ningún símbolo aunque debe estar definido
   cumpliéndose siempre que el recuento para el índice 0 debe ser
   siempre 0 (este es un requerimiento del codificador aritmético que
   estamos usando). Por tanto, si existen ALPHA_SIZE símbolos
   diferentes, existen ALPHA_SIZE+1 índices distintos. Nótese además
   que el tipo de dato asociado se escoge en relación con el valor
   MAX_CUM_COUNT. */
unsigned short prob[ALPHA_SIZE+1];

/* Recuentos acumulados de los índices. El codificador aritmético
   necesita que la entrada cum_prob[0] almacene el recuento acumlado
   de todos los símbolos. Nótese además que el tipo de dato asociado
   se escoge en relación con el valor MAX_CUM_COUNT. */
unsigned short cum_prob[ALPHA_SIZE+1];

/* Símbolo codificado. */
int symbol;

/* Indice del símbolo codificado. */
int _index;

/* Convierte un símbolo en un índice. */
int find_index(int symbol) {
  return symbol+1;
}

/* Convierte un índice en un símbolo. */
int find_symbol(int index) {
  return index-1;
}

/* Dadas las probabilidades de los símbolos en "prob", calcula las
   probabilidades acumuladas en "cum_prob". */
void compute_cumulative_probs() {
  int i;
  int cum = 0;
  for(i=ALPHA_SIZE; i>=0; i--) {
    cum_prob[i] = cum;
    cum += prob[i];
  }
}

/* Inicializa el modelo probabilistico. Todos los símbolos son,
   inicialmente, equiprobables. */
void init_model() {
  int i;
  for(i=0; i<ALPHA_SIZE; i++) {
    prob[find_index(i)] = 1;
  }
  compute_cumulative_probs();
}

/* Escala las probabilides de los símbolos dividiendo entre dos sus
   recuentos, redondeando al entero superior más cercano. */
void scale_probs() {
  int i;
  for (i = ALPHA_SIZE; i>=0; i--) {
    prob[i] = (prob[i]+1)/2;
  }
  fprintf(stderr,"S");
}

/* Comprueba si hay que escalar los recuentos de los símbolos, y si es
   así, lo hace. */
void test_if_scale() {
  if (cum_prob[0]==MAX_CUM_COUNT) {
    scale_probs();
    compute_cumulative_probs();
  }
}

/* Incrementa la probabilidad del símbolo asociado a "index". */
void increment_prob_of_index(int index) {
  prob[index]++;
  while(index>0) {
    cum_prob[--index]++;
  }
}

/* Actualiza el modelo incrementando el recuento del símbolo
   asociado a "index". */
void update_model() {
  test_if_scale();
  increment_prob_of_index(_index);
}

/* Finaliza el modelo probabilístico. */
void finish_model() {
  fprintf(stderr,"\n");
}

/* Codifica el stream de datos que entra por la entrada estándar y
   produce el code-stream sobre la salida estándar. */
void encode_stream(int argc, char *argv[]) {
  init_model();
  init_encoder();
  for(;;) {
    symbol = getchar();
    if(symbol==EOF) break;
    _index = find_index(symbol);
    encode_index(_index, cum_prob);
    update_model();
  }
  encode_index(find_index(EOS), cum_prob);
  finish_encoder();
  finish_model();
}

/* Realiza el proceso inverso a encode_stream(). */
void decode_stream(int argc, char *argv[]) {
  init_model();
  init_decoder();
  for(;;) {
    _index = decode_index(cum_prob);
    symbol = find_symbol(_index);
    if(symbol==EOS) break;
    putchar(symbol);
    update_model();
  }
  finish_decoder();
  finish_model();
}
