/*
 * model_a0.c
 *
 * Un modelo probabil�stico de orden 0.
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

/* Tama�o del alfabeto fuente. Los 256 caracteres posibles y el
   s�mbolo EOS (End Of Stream). */
#define ALPHA_SIZE 257

/* model_0.h */

/* C�digo de compresi�n que indica el fin del stream de datos. Su
   posici�n dentro del alfabeto fuente ser� siempre al final del
   mismo. */
#define EOS (ALPHA_SIZE-1)

/* M�ximo recuento acumulado permitido. Este valor afecta a la
   precisi�n del modelo probabil�stico a la hora de calcular las
   probabilidades de los s�mbolos. */
#define MAX_CUM_COUNT 16383

/* Probabilidad (en forma de recuento) de los �ndices. Cada �ndice
   est� asociado a un s�mbolo diferente, cumpli�ndose que el �ndice 0
   no se pude usar para ning�n s�mbolo aunque debe estar definido
   cumpli�ndose siempre que el recuento para el �ndice 0 debe ser
   siempre 0 (este es un requerimiento del codificador aritm�tico que
   estamos usando). Por tanto, si existen ALPHA_SIZE s�mbolos
   diferentes, existen ALPHA_SIZE+1 �ndices distintos. N�tese adem�s
   que el tipo de dato asociado se escoge en relaci�n con el valor
   MAX_CUM_COUNT. */
unsigned short prob[ALPHA_SIZE+1];

/* Recuentos acumulados de los �ndices. El codificador aritm�tico
   necesita que la entrada cum_prob[0] almacene el recuento acumlado
   de todos los s�mbolos. N�tese adem�s que el tipo de dato asociado
   se escoge en relaci�n con el valor MAX_CUM_COUNT. */
unsigned short cum_prob[ALPHA_SIZE+1];

/* S�mbolo codificado. */
int symbol;

/* Indice del s�mbolo codificado. */
int _index;

/* Convierte un s�mbolo en un �ndice. */
int find_index(int symbol) {
  return symbol+1;
}

/* Convierte un �ndice en un s�mbolo. */
int find_symbol(int index) {
  return index-1;
}

/* Dadas las probabilidades de los s�mbolos en "prob", calcula las
   probabilidades acumuladas en "cum_prob". */
void compute_cumulative_probs() {
  int i;
  int cum = 0;
  for(i=ALPHA_SIZE; i>=0; i--) {
    cum_prob[i] = cum;
    cum += prob[i];
  }
}

/* Inicializa el modelo probabilistico. Todos los s�mbolos son,
   inicialmente, equiprobables. */
void init_model() {
  int i;
  for(i=0; i<ALPHA_SIZE; i++) {
    prob[find_index(i)] = 1;
  }
  compute_cumulative_probs();
}

/* Escala las probabilides de los s�mbolos dividiendo entre dos sus
   recuentos, redondeando al entero superior m�s cercano. */
void scale_probs() {
  int i;
  for (i = ALPHA_SIZE; i>=0; i--) {
    prob[i] = (prob[i]+1)/2;
  }
  fprintf(stderr,"S");
}

/* Comprueba si hay que escalar los recuentos de los s�mbolos, y si es
   as�, lo hace. */
void test_if_scale() {
  if (cum_prob[0]==MAX_CUM_COUNT) {
    scale_probs();
    compute_cumulative_probs();
  }
}

/* Incrementa la probabilidad del s�mbolo asociado a "index". */
void increment_prob_of_index(int index) {
  prob[index]++;
  while(index>0) {
    cum_prob[--index]++;
  }
}

/* Actualiza el modelo incrementando el recuento del s�mbolo
   asociado a "index". */
void update_model() {
  test_if_scale();
  increment_prob_of_index(_index);
}

/* Finaliza el modelo probabil�stico. */
void finish_model() {
  fprintf(stderr,"\n");
}

/* Codifica el stream de datos que entra por la entrada est�ndar y
   produce el code-stream sobre la salida est�ndar. */
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
