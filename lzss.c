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
 * Tama�o del c�digo que indican la posici�n de la cadena en el
 * diccionario. Determina el tama�o la ventana que contiene el texto
 * que se ha codificado anteriormente (diccionario) el y texto que va
 * a codificarse (look-ahead buffer).
 */
#define INDEX_SIZE 12
#define WINDOW_SIZE (1<<INDEX_SIZE)

/* 
 * Tama�o del c�digo que indica la longitud de la cadena
 * encontrada. Determina el tama�o del look-ahead buffer.
 */
#define LENGTH_SIZE 4
#define RAW_LOOK_AHEAD_SIZE (1<<LENGTH_SIZE)

/*
 * Tama�o m�nimo de la cadena a codificar, medido en bytes. Se utiliza
 * para decidir si se env�an c�digos ijk o s�lo s�mbolos
 * k. T�picamente MIN_ENCODED_STRING_SIZE valdr� 1, lo que signfica
 * que s�lo si concatenamos al menos 2 s�mbolos utilizaremos un c�digo
 * ijk.
 */
#define MIN_ENCODED_STRING_SIZE ((1 +INDEX_SIZE+LENGTH_SIZE)/9)

/*
 * Tama�o efectivo del look-ahead buffer. Puesto que en la pr�ctica no
 * van a codificase cadenas de menos de 2 s�mbolos, es posible
 * reajustar el tama�o del look-ahead buffer sum�ndole 2. De esta
 * manera, cuando envi�mos un c�digo ijk donde j=0, en realidad
 * estaremos indicando una longitud real de 2. N�tese que esto tambi�n
 * provoca que el tama�o real del look-ahead buffer sea de 17 s�mbolos
 * cuando RAW_LOOK_AHEAD_BUFFER == 16.
*/
#define LOOK_AHEAD_SIZE (RAW_LOOK_AHEAD_SIZE + MIN_ENCODED_STRING_SIZE)

/*
 * Indice del nodo que apunta a la ra�z del �rbol binario.
 */
#define TREE_ROOT WINDOW_SIZE

/*
 * C�digo de compresi�n que indica el fin del stream de datos.
 */
#define END_OF_STREAM 0

/*
 * Indica que el nodo del �rbol todav�a no apunta a ninguna
 * cadena. Esto es lo mismo que codificar la cadena vac�a.
 */
#define UNUSED 0

/*
 * Calcula el m�dulo del entero "a" usando INDEX_SIZE bits de
 * precisi�n.
 */
#define MOD_WINDOW(a) ((a) & (WINDOW_SIZE-1))

/*
 * Diccionario y look-ahead buffer. Cuando comparemos la cadena que
 * almacena el look-ahead buffer con la que est� en la posici�n "p"
 * del diccionario, estaremos comparando dicha cadena con la que
 * comienza a partir de dicha direcci�n "p".
 */
unsigned char window[WINDOW_SIZE];

/*
 * Arbol binario de todas las cadenas que hay en la ventana ordenadas
 * lexicogr�ficamente. Cada nodo contiene 3 �ndices que apuntan a una
 * posici�n en la ventana, y por lo tanto, especifican una cadena. La
 * posici�n 0 de la ventana . La cadena a la que apunta
 * "smaller_child" es menor que la cadena a la que apunta "parent" y
 * menos que la cadena a la que apunta "larger_child".
 *
 * Por motivos de eficiencia de c�lculo, tree[WINDOW_SIZE] es un nodo
 * especial que se utiliza para localizar la ra�z del �rbol. Este
 * elemento no apunta a ninguna frase (como hacen el resto de nodos
 * del �rbol) Este nodo representa adem�s al c�digo END_OF_STREAM.
 *
 * Para comprender mejor c�mo funciona este estructura, a continuaci�n
 * se presenta un ejemplo de c�mo estar�a el �rbol cuando se ha
 * introducido en �l la cadena "ababcbababaaaaaaaa", siendo el tama�o
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
 * Inicializa el �rbol con el diccionario vac�o.
 * T�picamente r=1. As�, tras ejecutar este c�digo el �rbol queda como:
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
 * Despu�s:
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
 * Despu�s:
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
 * Localiza el siguiente nodo m�s peque�o que "node". Esto se hace
 * descendiendo primero por el hijo "smaller" de "node" y luego
 * bajando siempre por los hijos "larger", hasta llegar a una hoja del
 * �rbol. Se asume que "node" tiene un hijo m�s peque�o.
 */
int FindNextNode(int node) {
  int next;
  
  next = tree[ node ].smaller_child;
  while ( tree[ next ].larger_child != UNUSED )
    next = tree[ next ].larger_child;
  return( next );
}

/*
 * Elimina un nodo del �rbol.
 */
void DeleteString(int p) {
  int  replacement;
  /* Si el nodo a borrar no est� en el �rbol, no hacemos nada. */
  if ( tree[ p ].parent == UNUSED )
    return;
  /* Si el nodo a borrar s�lo tiene un hijo, hacemos una contracci�n
     del �rbol. */
  if ( tree[ p ].larger_child == UNUSED )
    ContractNode( p, tree[ p ].smaller_child );
  else if ( tree[ p ].smaller_child == UNUSED )
    ContractNode( p, tree[ p ].larger_child );
  /* Si el nodo a borrar tiene ambos descendientes. */
  else {
    /* Localizamos el siguiente nodo m�s peque�o que el nodo que
       intentamos borrar. */
    replacement = FindNextNode( p );
    /* Eliminaos el siguiente nodo m�s peque�o del �rbol. N�tese que
       el nodo "replacemente" nunca va a tener los dos descendientes,
       lo que evita entrar en m�s de un nivel de recursi�n. */
    DeleteString( replacement );
    /* Sustituimos el nodo que estamos intentanbo borrar por el que
       acabamos de localizar y eliminar el �rbol. */
    ReplaceNode( p, replacement );
  }
}

/*
 * Esta rutina inserta un nuevo nodo al �rbol binario y encuentra
 * (retornando) la mejor ocurrencia de la cadena buscada.
 */
int AddString(int new_node, int *match_position) {
  int i;
  int test_node;
  int delta;
  int match_length;
  int *child;
  
  /* Estamos insertando el s�mbolo END_OF_STREAM? */
  if ( new_node == END_OF_STREAM )
    return 0;
  /* Accedemos a la ra�z del �rbol y de ah� al primer nodo con datos
     almacenado. */
  test_node = tree[ TREE_ROOT ].larger_child;
  /* La longitud de la cadena encontrada es todav�a, 0. */
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
    /* Si hemos encontrado una cadena m�s larga. */
    if ( i >= match_length ) {
      match_length = i;
      *match_position = test_node;
      /* Si hemos encontrado todo el buffer de anticipaci�n en el
	 diccionario (ya no es posible buscar una cadena m�s
	 larga). */
      if ( match_length >= LOOK_AHEAD_SIZE ) {
	/* "new_node" reemplaza a "test_node" para manterner el �rbol
	   lo m�s peque�o posible, sin afectar a la tasa de
	   compresi�n. */
	ReplaceNode( test_node, new_node );
	return  match_length;
      }
    }
    /* Navegaci�n binaria sobre el �rbol. */
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
 * Realiza la compresi�n del stream.
 */
void encode_stream(int argc, char *argv[]) {
  int i;
  int c;
  int look_ahead_bytes;
  int current_position;
  int replace_count;
  int match_length;
  int match_position;

  /* Carga el buffer de anticipaci�n. */
  current_position = 1;
  for ( i = 0 ; i < LOOK_AHEAD_SIZE ; i++ ) {
    if ( ( c = getchar() ) == EOF )
      break;
    window[ current_position + i ] = (unsigned char) c;
  }
  look_ahead_bytes = i; /* look_ahead_bytes = 17, excepto al final de
			   la compresi�n. */

  /* Inicializa el �rbol de b�squeda binario. */
  InitTree( current_position );

  /* Longitud de la cadena encontrada. */
  match_length = 0;

  /* Posici�n de la cadena encontrada. */
  match_position = 0;

  /* Comienza la compresi�n. */
  while ( look_ahead_bytes > 0 ) {
    if ( match_length > look_ahead_bytes )
      match_length = look_ahead_bytes;
    /* Decidimos si enviar una "k" o un c�digo "ij". */
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

    /* Insertamos "replace_count" s�mbolos en el buffer de
       anticipaci�n y los eleiminados del diccionario.*/
    for ( i = 0 ; i < replace_count ; i++ ) {
      /* Eliminamos del �rbol binario de b�squeda los s�mbolos que
	 salen por la parte izquierda de la ventana deslizante. */
      DeleteString( MOD_WINDOW( current_position + LOOK_AHEAD_SIZE ) );
      /* Leemos los nuevos s�mbolos. */
      if ( ( c = getchar() ) == EOF )
	look_ahead_bytes--;
      else
	window[ MOD_WINDOW( current_position + LOOK_AHEAD_SIZE ) ]
	  = (unsigned char) c;
      /* Actualizamos el "puntero" por el que vamos comprimiendo el
	 stream de datos. No olvidemos que lo procesamos usando una
	 cola circular. */
      current_position = MOD_WINDOW( current_position + 1 );
      /* Insertamos en el �rbol binario de b�squeda los nuevos
	 s�mbolos. */
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
 * Realiza la descompresi�n del stream.
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
      /* Le�do 1, un-encoded "k". */
      c = get_bits(8);
      putchar(c);
      window[ current_position ] = (unsigned char) c;
      current_position = MOD_WINDOW( current_position + 1 );
    } else {
      /* Le�do 0, "ij" code. */
      match_position = get_bits(INDEX_SIZE); /* "i" */
      if ( match_position == END_OF_STREAM )
	break;
      match_length = get_bits(LENGTH_SIZE); /* "j" */
      match_length += MIN_ENCODED_STRING_SIZE;
      /* Copiamos a la salida "j" caracteres a partir de la posici�n
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

