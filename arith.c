/*
 * arithmetic_coding.c
 *
 * Un codificador artim�tico.
 *
 * Referencias:
 *
 * I. H. Witten, R. M. Neal, and J. G. Cleary, "Arithmetic coding for
 * data compression," Commun. ACM, vol. 30, no. 6, pp. 520--540,
 * Jun. 1987.
 */

#include <stdio.h>
#include "bitio.h"
#include "vlc.h"

/*
 * Precisi�n aritm�tica. Este valor afecta al tama�o m�nimo del
 * intervalo de codificaci�n que es posible crear en una iteraci�n del
 * proceso de transmisi�n incremental.
 */
#define BIT_ACCURACY 16

typedef long code_value;

/* Valores cr�ticos en el intervalo de divisi�n. */
#define _0_99 (((long)1<<BIT_ACCURACY)-1) /* 0.999... */
#define _0_25 (_0_99/4+1)                 /* 0.25 */
#define _0_50 (_0_25*2)                   /* 0.50 */
#define _0_75 (_0_25*3)                   /* 0.75 */

/* Extremos del intervalo de codificaci�n actual. */
static code_value low, high;

/* Bits del c�digo artim�tico actualmente contemplados en la
   descodificaci�n. */
static code_value value;

/* N�mero de bits (opuestos) emitidos tras el siguiente bit. */
static long bits_to_follow;

/* Inicializa el codificador. */
void init_encoder() {
  low = 0;
  high = _0_99;
  bits_to_follow = 0;
}

/* Inicializa el descodificador. */
void init_decoder() {
  int i;
  value = 0;
  for (i = 0; i<BIT_ACCURACY; i++) {
    value = 2*value;
    if(get_bit()) {
      value += 1;
    }
  }
  low = 0;
  high = _0_99;
}


/* Emite un bit y a continuaci�n "bits_to_follow" bits contrarios. */
static void bit_plus_follow(int bit) {
  put_bit(bit);
  while (bits_to_follow>0) {
    put_bit(!bit);
    bits_to_follow -= 1;
  }
}

/* Codifica el �ndice "index" usando el espacio de recuentos
   acumulados "cum_freq". */
void encode_index(int index, unsigned short *cum_prob) {
  /* Tama�o del intervalo de codificaci�n actual. */
  long range = (long)(high-low)+1;
  
  /* Seleccionamos el siguiente intervalo. */
  high = low + (range*cum_prob[index-1])/cum_prob[0]-1;
  low  = low + (range*cum_prob[index  ])/cum_prob[0];
  
  /* Lazo de transmisi�n incremental. Este lazo se ejecuta tantas
     veces como sea necesario para que "low" y "high" no coincidan en
     sus bits m�s significativos. */
  for (;;) {
    if (high<_0_50) {
      /* El bit m�s significativo de "low" y "high" es 0. Este bit
	 puede ser transmitido. */
      bit_plus_follow(0);
    }
    else if (low>=_0_50) {
      /* El bit m�s significativo de "low" y "high" es 1. Este bit
	 puede ser transmitido. */
      bit_plus_follow(1);
      
      /* Evitamos el desbordamiento de los registros. */
      low -= _0_50;
      high -= _0_50;
    }
    else if (low>=_0_25 && high<_0_75) {
      /* El bit m�s significativo de "low" y "high" no coinciden y por lo tanto no pueden ser transmitidos. En esta situaci�n "low"=01... y "high"=10... */
      bits_to_follow += 1;
      
      /* Evitamos el desbordamiento. */
      low -= _0_25;
      high -= _0_25;
    }
    /* Ning�n bit se "low" y "high" coinciden porque el intervalo de
       demasiado grande. */
    else break;
    
    /* Escalamos el intervalo de codificaci�n. */
    low = 2*low;
    high = 2*high+1;
  }
}


/* Descodifica el siguiente �ndice usando el espacio de recuentos
   acumulados "cum_freq". */
int decode_index(unsigned short *cum_prob) {
  /* S�mbolo descodificado. */
  int index;
  
  /* Tama�o del intervalo de codificaci�n actual. */
  long range = (long)(high-low)+1;
  
  /* Recuento acumulado para "value". */
  int cum = (int)((((long)(value-low)+1)*cum_prob[0]-1)/range);
  
  /* Encontramos el s�mbolo. */
  for (index = 1; cum_prob[index]>cum; index++);
  
  /* Seleccionamos el intervalo de codificaci�n asociado al s�mbolo
     descodificado, tal y como hizo el codificador. */
  high = low + (range*cum_prob[index-1])/cum_prob[0]-1;
  low  = low + (range*cum_prob[index  ])/cum_prob[0];
  
  /* Recepci�n incremental. */
  for (;;) {
    if (high<_0_50) {
      /* nothing */
    }
    else if (low>=_0_50) {
      /* Vamos a expandir la mitad superior del intervalo de
	 codificaci�n. Restamos el offset 0.5. */
      value -= _0_50;
      low -= _0_50;
      high -= _0_50;
    }
    else if (low>=_0_25 && high<_0_75) {
      /* Vamos a expandir la mitad media del intervalo de
	 codificaci�n. Restamos el offset 0.25. */
      value -= _0_25;
      low -= _0_25;
      high -= _0_25;
    }
    else break;
    
    /* Escalamos el intervalo de codificaci�n. */
    low = 2*low;
    high = 2*high+1;
    
    /* Le�mos el siguiente bit de c�digo aritm�tico. */
    value = 2*value;
    if(get_bit()) {
      value += 1;
    }
  }
  return index;
}

/* Finaliza el codificador. */
void finish_encoder() {
  /* Transmitimos dos bits que seleccionan el cuarto que el
     intervalo de codificaci�n actualmente contiene. */
  bits_to_follow += 1;
  if (low<_0_25) bit_plus_follow(0);
  else bit_plus_follow(1);
  flush();
}

/* Finaliza el descodificador. */
void finish_decoder() {
}
