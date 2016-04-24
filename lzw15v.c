/*
 * lzw15v.c
 *
 * The LZW algorithm.
 *
 * Referencias:
 *
 * T. A. Welch, IEEE Computer, Vol. 17, pp 8-19. 1984.
 * M. Nelson and J.-L. Gailly, The Data Compression Book. 1995.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitio.h"

/*
 * Número máximo de bits de un código de compresión "w".
 */
#define MAX_CODE_SIZE_IN_BITS 15

/*
 * Valor máximo que puede tomar un código de compresión.
 */
#define MAX_CODE ((1<<MAX_CODE_SIZE_IN_BITS)-1)

/*
 * Define el tamaño del diccionario. Como se utiliza hashing y cuando
 * aparece una colisión se utiliza una búsqueda secuencial, interesa
 * utilizar un número primo suficientemente grande. A continuación se
 * muestran algunas sugerencias próximas a potencias de dos:
 * 4999, 8999, 17989, 35023, 69997, 139999.
 */
#define TABLE_SIZE                 35023L

/*
 * Indica el fin de la compresión.
 */
#define END_OF_STREAM              256

/*
 * Indica ...
 */
#define BUMP_CODE                  257

/*
 * Indica un vaciado del diccionario.
 */
#define FLUSH_CODE                 258

/*
 * Primera entrada en el diccionario que almacena una cadena.
 */
#define FIRST_CODE                 259

/*
 * Entrada vacía.
 */
#define UNUSED                     -1

/*
 * La siguiente estructura de datos declara (de forma estática) el
 * diccionario. Como puede apreciarse, se trata de una tabla de ternas
 * "code_value", "parent_code" y "character", donde cada terna
 * especifica una cadena diferente. El campo "code_value" es el código
 * de compresión asociado a la cadena "parent_code""character", es
 * decir, el índice de la cadena "parent_code""character" en el
 * diccionario. Recuerdese además que "code_value"s menores que 256
 * codifican símbolos (raíces).
 */
struct dictionary {
    int code_value;
    int parent_code;
    char k;
} dict[TABLE_SIZE];

/*
 * Contiene la cadena "w" descodificada.
 */
char decode_stack[TABLE_SIZE];

/*
 * Siguiente código insertado en el diccionario.
 */
unsigned int next_w;

/*
 * Tamaño actual del código de compresión.
 */
int current_code_bits;

/*
 * Código de compresión que incrementará el tamaño del código de
 * compresión.
 */
unsigned int next_bump_code;

/*
 * Inicializa el diccionario y otras variables globales.
 */
void InitializeDictionary()
{
    unsigned int i;

    for ( i = 0 ; i < TABLE_SIZE ; i++ )
        dict[ i ].code_value = UNUSED;
    next_w = FIRST_CODE;
    current_code_bits = 9;
    next_bump_code = 511;
}

/*
 * Busca en el diccionario la cadena "wk". Se utiliza una función hash
 * que depende "w" y de "k", que se relacionan mediante la operación
 * XOR. En caso de aparecer una colisión se busca, tantas veces como
 * sea necesario, en la entrada "index*2 mod TABLE_SIZE", donde
 * "index" el la posición esperrada de la cadena "wk" en el
 * diccionario.
 */
unsigned int find_child_node(int parent_code, int child_k) {
  unsigned int index;
  unsigned int offset;
  
  index = ( child_k << ( MAX_CODE_SIZE_IN_BITS - 8 ) ) ^ parent_code;
  if ( index == 0 )
    offset = 1;
  else
    offset = TABLE_SIZE - index;
  for ( ; ; ) {
    if ( dict[ index ].code_value == UNUSED )
      /* Entrada vacía. */
      return index;
    if ( dict[ index ].parent_code == parent_code &&
	 dict[ index ].k == (char) child_k )
      /* Cadena encontrada. */
      return index;
    /* Colisión. */
    if ( (int) index >= offset )
      index -= offset;
    else
      index += TABLE_SIZE - offset;
  }
}

/*
 * This routine decodes a string from the dictionary, and stores it
 * in the decode_stack data structure.  It returns a count to the
 * calling program of how many characters were placed in the stack.
 */

unsigned int string(unsigned int count, unsigned int w) {
  while ( w > 255 ) {
    decode_stack[ count++ ] = dict[ w ].k;
    w = dict[ w ].parent_code;
  }
  decode_stack[ count++ ] = (char) w;
  return( count );
}

/*
 * The compressor is short and simple.  It reads in new symbols one
 * at a time from the input file.  It then  checks to see if the
 * combination of the current symbol and the current code are already
 * defined in the dictionary.  If they are not, they are added to the
 * dictionary, and we start over with a new one symbol code.  If they
 * are, the code for the combination of the code and character becomes
 * our new code.  Note that in this enhanced version of LZW, the
 * encoder needs to check the codes for boundary conditions.
 */

void encode_stream(int argc, char *argv[]) {
  int k;
  int w;
  unsigned int index;
  
  InitializeDictionary();
  if ((w=getchar())==EOF)
    /* Fichero de entrada vacío! */
    w = END_OF_STREAM;
  while ((k=getchar())!=EOF) {
    /* Buscamos "wk" en el diccionario. */
    index = find_child_node(w, k);

    if ( dict[ index ].code_value != - 1 )
      /* "wk" está en el diccionario. */

      /* w <- dirección de "wk". */
      w = dict[ index ].code_value;
    else {
      /* "wk" no está en el diccionario. */

      /* Escribimos "w" a la salida. */
      put_bits(w, current_code_bits);

      /* Insertamos "wk" en el diccionario. */
      dict[ index ].code_value = next_w++;
      dict[ index ].parent_code = w;
      dict[ index ].k = (char) k;

      /* w <- k. */
      w = k;

      if (next_w > MAX_CODE) {
	/* Vaciamos el diccionario. */
	put_bits(FLUSH_CODE, current_code_bits);
	InitializeDictionary();
      } else if (next_w > next_bump_code) {
	/* Aumentamos el tamaño del código en 1 bit. */
	put_bits(BUMP_CODE, current_code_bits);
	current_code_bits++;
	next_bump_code <<= 1;
	next_bump_code |= 1;
      }
    }
  }
  /* EOS. */
  put_bits(w, current_code_bits);
  put_bits(END_OF_STREAM, current_code_bits);
  flush();
}

/*
 * The file expander operates much like the encoder.  It has to
 * read in codes, the convert the codes to a string of characters.
 * The only catch in the whole operation occurs when the encoder
 * encounters a CHAR+STRING+CHAR+STRING+CHAR sequence.  When this
 * occurs, the encoder outputs a code that is not presently defined
 * in the table.  This is handled as an exception.  All of the special
 * input codes are handled in various ways.
 */

void decode_stream(int argc, char *argv[]) {
  unsigned int w;
  unsigned int prev_w;
  int k;
  unsigned int count;
  
  for ( ; ; ) {
    InitializeDictionary();
    /* prev_w <- primer código de entrada. */
    prev_w = get_bits(current_code_bits);
    if ( prev_w == END_OF_STREAM )
      return;
    /* Escribimos prev_w a la salida. */
    putchar(prev_w);
    /* k <- prev_w. */
    k = prev_w;
    for ( ; ; ) {
      /* w <- siguiente códido de entrada. */
      w = get_bits(current_code_bits);
      /* Mientras existan códigos de entrada. */
      if ( w == END_OF_STREAM )
	return;
      if ( w == FLUSH_CODE )
	break;
      if ( w == BUMP_CODE ) {
	current_code_bits++;
	continue;
      }
      if ( w >= next_w ) {
	/* Si w no está en el diccionario. */
	/* Escribir string(w)+k a la salida. */
	decode_stack[ 0 ] = (char) k;
	count = string( 1, prev_w );
      }
      else
	/* Si w está en el diccionario. */
	/* Escribir string(w) a la salida. */
	count = string( 0, w );
      /* k <- primer símbolo emitido en la salida anterior. */
      k = decode_stack[ count - 1 ];
      while ( count > 0 )
	putchar(decode_stack[--count]);
      /* Insertar wk en el diccionario. */
      dict[ next_w ].parent_code = prev_w;
      dict[ next_w ].k = (char) k;
      next_w++;
      /* prev_w <- w. */
      prev_w = w;
    }
  }
}
