/*
 * lzss.c
 *
 * Storer & Szymanski implementation of the LZ77 algorithm.
 *
 * Referencias:
 *
 * J. Ziv and A. Lempel, IEEE Trans. IT-23, 337-343 (1977).
 * J. A. Storer and T. G. Szymanski, J. ACM, 29, 928-951 (1982).
 * T. C. Bell, IEEE Trans. COM-34, 1176-1182 (1986).
 * M. Nelson and J.-L. Gailly, The Data Compression Book. 1995.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bitio.h"

/*
 * Tamaño del código que indican la posición de la cadena en el
 * diccionario. Determina el tamaño la ventana que contiene el texto
 * que se ha codificado anteriormente (diccionario) el y texto que va
 * a codificarse (look-ahead buffer).
 */
#define INDEX_SIZE 12
#define WINDOW_SIZE (1<<INDEX_SIZE)

/* 
 * Tamaño del código que indica la longitud de la cadena
 * encontrada. Determina el tamaño del look-ahead buffer.
 */
#define LENGTH_SIZE 4
#define RAW_LOOK_AHEAD_SIZE (1<<LENGTH_SIZE)

/*
 * Tamaño mínimo de la cadena a codificar, medido en bytes. Se utiliza
 * para decidir si se envían códigos ijk o sólo símbolos
 * k. Típicamente MIN_ENCODED_STRING_SIZE valdrá 1, lo que signfica
 * que sólo si concatenamos al menos 2 símbolos utilizaremos un código
 * ijk.
 */
#define MIN_ENCODED_STRING_SIZE ((1 +INDEX_SIZE+LENGTH_SIZE)/9)

/*
 * Tamaño efectivo del look-ahead buffer. Puesto que en la práctica no
 * van a codificase cadenas de menos de 2 símbolos, es posible
 * reajustar el tamaño del look-ahead buffer sumándole 2. De esta
 * manera, cuando enviémos un código ijk donde j=0, en realidad
 * estaremos indicando una longitud real de 2. Nótese que esto también
 * provoca que el tamaño real del look-ahead buffer sea de 17 símbolos
 * cuando RAW_LOOK_AHEAD_BUFFER == 16.
*/
#define LOOK_AHEAD_SIZE (RAW_LOOK_AHEAD_SIZE + MIN_ENCODED_STRING_SIZE)

/*
 * Indice del nodo que apunta a la raíz del árbol binario.
 */
#define TREE_ROOT WINDOW_SIZE

/*
 * Código de compresión que indica el fin del stream de datos.
 */
#define END_OF_STREAM 0

/*
 * Indica que el nodo del árbol todavía no apunta a ninguna
 * cadena. Esto es lo mismo que codificar la cadena vacía.
 */
#define UNUSED 0

/*
 * Calcula el módulo del entero "a" usando INDEX_SIZE bits de
 * precisión.
 */
#define MOD_WINDOW(a) ((a) & (WINDOW_SIZE-1))

/*
 * Diccionario y look-ahead buffer. Cuando comparemos la cadena que
 * almacena el look-ahead buffer con la que está en la posición "p"
 * del diccionario, estaremos comparando dicha cadena con la que
 * comienza a partir de dicha dirección "p".
 */
unsigned char window[WINDOW_SIZE];

/*
 * Arbol binario de todas las cadenas que hay en la ventana ordenadas
 * lexicográficamente. Cada nodo contiene 3 índices que apuntan a una
 * posición en la ventana, y por lo tanto, especifican una cadena. La
 * posición 0 de la ventana . La cadena a la que apunta
 * "smaller_child" es menor que la cadena a la que apunta "parent" y
 * menos que la cadena a la que apunta "larger_child".
 *
 * Por motivos de eficiencia de cálculo, tree[WINDOW_SIZE] es un nodo
 * especial que se utiliza para localizar la raíz del árbol. Este
 * elemento no apunta a ninguna frase (como hacen el resto de nodos
 * del árbol) Este nodo representa además al código END_OF_STREAM.
 *
 * Para comprender mejor cómo funciona este estructura, a continuación
 * se presenta un ejemplo de cómo estaría el árbol cuando se ha
 * introducido en él la cadena "ababcbababaaaaaaaa", siendo el tamaño
 * del look-ahead buffer igual a 8.
 * 
 * Window="ababcbababaaaaaaaa"
 *
 * ------------12345678
 * cadena  1: "ababcbab"
 * cadena  2: "babcbaba"
 * cadena  3: "abcbabab"
 * cadena  4: "bcbababa"
 * cadena  5: "cbababaa"
 * cadena  6: "bababaaa"
 * cadena  7: "ababaaaa"
 * cadena  8: "babaaaaa"
 * cadena  9: "abaaaaaa"
 * cadena 10: "baaaaaaa"
 * cadena 11: "aaaaaaaa"
 *
 *
 *                   nodo 4096
 *       smaller +-----+-----+ larger
 *               |           |
 *                         nodo 1
 *                       "ababcbab"
 *                     +-----+-----+
 *                     |           |
 *                  nodo 7       nodo 2
 *                "ababaaaa"   "babcbaba"
 *                +----+-+   +-----+-----+
 *                |      |   |           |
 *              nodo 9     nodo 3      nodo 4
 *            "abaaaaaa" "abcbabab"  "bcbababa"
 *            +---+-+   +----+----+    +-+----+
 *            |     |   |         |    |      |
 *         nodo 11              nodo 6     nodo 5
 *       "aaaaaaaa"           "bababaaa" "cbababaa"
 *                            +---+---+
 *                            |       |
 *                          nodo 8
 *                        "babaaaaa"
 *                        +---+---+
 *                        |       |
 *                      nodo 10
 *                    "baaaaaaa"
 */
struct {
  int parent;
  int smaller_child;
  int larger_child;
} tree[WINDOW_SIZE + 1];

/*
 * Inicializa el árbol con el diccionario vacío.
 * Típicamente r=1. Así, tras ejecutar este código el árbol queda como:
 *
 *     nodo 4096
 *        +----+
 *             |
 *           nodo 1
 *        +----+----+
 *        |         |
 *        0         0
 */
void InitTree(int r) {
  tree[ TREE_ROOT ].larger_child = r;
  tree[ r ].parent = TREE_ROOT;
  tree[ r ].larger_child = UNUSED;
  tree[ r ].smaller_child = UNUSED;
}

/*
 * Antes:
 *                     parent
 *                 +-----+-----+
 *                 |           |
 *                old
 *             +---+---+
 *             |       |
 *            new   "unused"
 *         +---+---+
 *         |       |
 *         A       B
 *
 * Después:
 *                     parent
 *                 +-----+-----+
 *                 |           |
 *                new
 *             +---+---+
 *             |       |
 *             A       B
 */
void ContractNode(int old_node, int new_node) {
  tree[ new_node ].parent = tree[ old_node ].parent;
  if ( tree[ tree[ old_node ].parent ].larger_child == old_node )
    tree[ tree[ old_node ].parent ].larger_child = new_node;
  else
    tree[ tree[ old_node ].parent ].smaller_child = new_node;
  tree[ old_node ].parent = UNUSED;
}

/*
 * Antes:
 *                    parent           new
 *                 +-----+-----+    +---+---+
 *                 |           |    |       |
 *                old
 *             +---+---+
 *             |       |
 *             A       B
 * Después:
 *                    parent
 *                 +-----+-----+
 *                 |           |
 *                new
 *             +---+---+
 *             |       |
 *             A       B
 */
void ReplaceNode(int old_node, int new_node) {
  int parent;
  
  parent = tree[ old_node ].parent;
  if ( tree[ parent ].smaller_child == old_node )
    tree[ parent ].smaller_child = new_node;
  else
    tree[ parent ].larger_child = new_node;
  tree[ new_node ] = tree[ old_node ];
  tree[ tree[ new_node ].smaller_child ].parent = new_node;
  tree[ tree[ new_node ].larger_child ].parent = new_node;
  tree[ old_node ].parent = UNUSED;
}

/*
 * Localiza el siguiente nodo más pequeño que "node". Esto se hace
 * descendiendo primero por el hijo "smaller" de "node" y luego
 * bajando siempre por los hijos "larger", hasta llegar a una hoja del
 * árbol. Se asume que "node" tiene un hijo más pequeño.
 */
int FindNextNode(int node) {
  int next;
  
  next = tree[ node ].smaller_child;
  while ( tree[ next ].larger_child != UNUSED )
    next = tree[ next ].larger_child;
  return( next );
}

/*
 * Elimina un nodo del árbol.
 */
void DeleteString(int p) {
  int  replacement;
  /* Si el nodo a borrar no está en el árbol, no hacemos nada. */
  if ( tree[ p ].parent == UNUSED )
    return;
  /* Si el nodo a borrar sólo tiene un hijo, hacemos una contracción
     del árbol. */
  if ( tree[ p ].larger_child == UNUSED )
    ContractNode( p, tree[ p ].smaller_child );
  else if ( tree[ p ].smaller_child == UNUSED )
    ContractNode( p, tree[ p ].larger_child );
  /* Si el nodo a borrar tiene ambos descendientes. */
  else {
    /* Localizamos el siguiente nodo más pequeño que el nodo que
       intentamos borrar. */
    replacement = FindNextNode( p );
    /* Eliminaos el siguiente nodo más pequeño del árbol. Nótese que
       el nodo "replacemente" nunca va a tener los dos descendientes,
       lo que evita entrar en más de un nivel de recursión. */
    DeleteString( replacement );
    /* Sustituimos el nodo que estamos intentanbo borrar por el que
       acabamos de localizar y eliminar el árbol. */
    ReplaceNode( p, replacement );
  }
}

/*
 * Esta rutina inserta un nuevo nodo al árbol binario y encuentra
 * (retornando) la mejor ocurrencia de la cadena buscada.
 */
int AddString(int new_node, int *match_position) {
  int i;
  int test_node;
  int delta;
  int match_length;
  int *child;
  
  /* Estamos insertando el símbolo END_OF_STREAM? */
  if ( new_node == END_OF_STREAM )
    return 0;
  /* Accedemos a la raíz del árbol y de ahí al primer nodo con datos
     almacenado. */
  test_node = tree[ TREE_ROOT ].larger_child;
  /* La longitud de la cadena encontrada es todavía, 0. */
  match_length = 0;
  for ( ; ; ) {
    /* "i" contiene el "match_length" actual. */
    for ( i = 0 ; i < LOOK_AHEAD_SIZE ; i++ ) {
      /* "delta" < 1 si la cadena almacenada en "new_node" es menor
	 que la de "test_node", "delta" = 0 si son iguales y "delta" >
	 1 si la cadena en "nee_node" es mayor que la de
	 "test_node". */
      delta = window[ MOD_WINDOW( new_node + i ) ] -
	window[ MOD_WINDOW( test_node + i ) ];
      if ( delta != 0 )
	break;
    }
    /* Si hemos encontrado una cadena más larga. */
    if ( i >= match_length ) {
      match_length = i;
      *match_position = test_node;
      /* Si hemos encontrado todo el buffer de anticipación en el
	 diccionario (ya no es posible buscar una cadena más
	 larga). */
      if ( match_length >= LOOK_AHEAD_SIZE ) {
	/* "new_node" reemplaza a "test_node" para manterner el árbol
	   lo más pequeño posible, sin afectar a la tasa de
	   compresión. */
	ReplaceNode( test_node, new_node );
	return  match_length;
      }
    }
    /* Navegación binaria sobre el árbol. */
    if ( delta >= 0 )
      child = &tree[ test_node ].larger_child;
    else
      child = &tree[ test_node ].smaller_child;
    if ( *child == UNUSED ) {
      *child = new_node;
      tree[ new_node ].parent = test_node;
      tree[ new_node ].larger_child = UNUSED;
      tree[ new_node ].smaller_child = UNUSED;
      return match_length;
    }
    test_node = *child;
  }
}

/*
 * Realiza la compresión del stream.
 */
void encode_stream(int argc, char *argv[]) {
  int i;
  int c;
  int look_ahead_bytes;
  int current_position;
  int replace_count;
  int match_length;
  int match_position;

  /* Carga el buffer de anticipación. */
  current_position = 1;
  for ( i = 0 ; i < LOOK_AHEAD_SIZE ; i++ ) {
    if ( ( c = getchar() ) == EOF )
      break;
    window[ current_position + i ] = (unsigned char) c;
  }
  look_ahead_bytes = i; /* look_ahead_bytes = 17, excepto al final de
			   la compresión. */

  /* Inicializa el árbol de búsqueda binario. */
  InitTree( current_position );

  /* Longitud de la cadena encontrada. */
  match_length = 0;

  /* Posición de la cadena encontrada. */
  match_position = 0;

  /* Comienza la compresión. */
  while ( look_ahead_bytes > 0 ) {
    if ( match_length > look_ahead_bytes )
      match_length = look_ahead_bytes;
    /* Decidimos si enviar una "k" o un código "ij". */
    if ( match_length <= MIN_ENCODED_STRING_SIZE ) {
      /* "k": Un-encoded output. */
      put_bit(1);
      put_bits(window[ current_position ], 8);
      replace_count = 1;
    } else {
      /* "ij": Encoded output. */
      put_bit(0);
      put_bits(match_position, INDEX_SIZE);
      put_bits((match_length - (MIN_ENCODED_STRING_SIZE + 1)),
	       LENGTH_SIZE );
      replace_count = match_length;
    }

    /* Insertamos "replace_count" símbolos en el buffer de
       anticipación y los eleiminados del diccionario.*/
    for ( i = 0 ; i < replace_count ; i++ ) {
      /* Eliminamos del árbol binario de búsqueda los símbolos que
	 salen por la parte izquierda de la ventana deslizante. */
      DeleteString( MOD_WINDOW( current_position + LOOK_AHEAD_SIZE ) );
      /* Leemos los nuevos símbolos. */
      if ( ( c = getchar() ) == EOF )
	look_ahead_bytes--;
      else
	window[ MOD_WINDOW( current_position + LOOK_AHEAD_SIZE ) ]
	  = (unsigned char) c;
      /* Actualizamos el "puntero" por el que vamos comprimiendo el
	 stream de datos. No olvidemos que lo procesamos usando una
	 cola circular. */
      current_position = MOD_WINDOW( current_position + 1 );
      /* Insertamos en el árbol binario de búsqueda los nuevos
	 símbolos. */
      if ( look_ahead_bytes )
	match_length = AddString( current_position, &match_position );
    }
  };
  /* EOF alcanzado. */
  put_bit(0);
  put_bits(END_OF_STREAM, INDEX_SIZE);
  flush();
}

/*
 * Realiza la descompresión del stream.
 */
void decode_stream(int argc, char *argv[]) {
  int i;
  int current_position;
  int c;
  int match_length;
  int match_position;
  
  current_position = 1;
  for ( ; ; ) {
    if (get_bit()) {
      /* Leído 1, un-encoded "k". */
      c = get_bits(8);
      putchar(c);
      window[ current_position ] = (unsigned char) c;
      current_position = MOD_WINDOW( current_position + 1 );
    } else {
      /* Leído 0, "ij" code. */
      match_position = get_bits(INDEX_SIZE); /* "i" */
      if ( match_position == END_OF_STREAM )
	break;
      match_length = get_bits(LENGTH_SIZE); /* "j" */
      match_length += MIN_ENCODED_STRING_SIZE;
      /* Copiamos a la salida "j" caracteres a partir de la posición
	 "i" del diccionario. */
      for ( i = 0 ; i <= match_length ; i++ ) {
	c = window[ MOD_WINDOW( match_position + i ) ];
	putchar(c);
	window[ current_position ] = (unsigned char) c;
	current_position = MOD_WINDOW( current_position + 1 );
      }
    }
  }
}

