/*
 * huff.c
 *
 * Un codificador de Huffman junto a un modelo probabilístico,
 * estático, de orden 0.
 *
 * Referencias:
 *
 * D. A. Huffman, Proceedings of the Institute of Radio Engineers,
 * Vol. 40, pp. 1098-1101. 1952.
 * M. Nelson and J.-L. Gailly, The Data Compression Book. 1995.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
//#include "bitio.h"
//#include "main.h"

/*
 * The NODE structure is a node in the Huffman decoding tree.  It has a
 * count, which is its weight in the tree, and the node numbers of its
 * two children.
 */
typedef struct tree_node {
  unsigned int count;
  int child_0;
  int child_1;
} NODE;

/*
 * A Huffman tree is set up for decoding, not encoding. When encoding,
 * I first walk through the tree and build up a table of codes for
 * each symbol. The codes are stored in this CODE structure.
 */
typedef struct code {
  unsigned int code;
  int code_bits;
} CODE;

/*
 * Mantiene una copia de la entrada estándar. Recuérdese que este
 * programa funciona en dos pasadas. En la primera se calcula el
 * recuento de cada byte en el fichero a comprimir. En la segunda se
 * comprime. "tmp_file" almacena una copia de la entrada estándar en
 * un fichero temporal para poder recorrer el stream de entrada dos
 * veces.
 */
FILE *tmp_file;

/*
 * The special EOS symbol is 256, the first available symbol after all
 * of the possible bytes.  When decoding, reading this symbols
 * indicates that all of the data has been read in.
 */
#define END_OF_STREAM 256

/*
 * Comprime el stream de entrada.
 */
void encode_stream() {
  unsigned long counts[256];
  NODE nodes[514];
  CODE codes[257];
  int root_node;
  count_bytes(counts);
  scale_counts( counts, nodes );
  output_counts(nodes);
  root_node = build_tree( nodes );
  convert_tree_to_code( nodes, codes, 0, 0, root_node );
  compress_data(codes);
}

/*
 * Expande el stream de entrada.
 */
void decode_stream() {
  NODE nodes[514];
  int root_node;
  input_counts(nodes);
  root_node = build_tree( nodes );
  expand_data(nodes, root_node);
}

/*
 * This routine counts the frequency of occurence of every byte in
 * the input file.
 */
count_bytes(unsigned long *counts) {
  int c;
  char tmp_filename[80], number[5];
  sprintf(number,"%d", getpid()%1000);
  strcpy(tmp_filename, "/tmp/");
  strcat(tmp_filename, number);
  tmp_file = fopen(tmp_filename, "w+");
  if(!tmp_file) {
    fprintf(stderr, "huff: imposible abrir (%s)\n", tmp_filename);
    exit(1);
  }
  while ( (c = getchar()) != EOF ) {
    counts[ c ]++;
    putc(c, tmp_file);
  }
  fflush(tmp_file);
  rewind(tmp_file);
}

/*
 * In order to limit the size of my Huffman codes to 16 bits, I scale
 * my counts down so they fit in an unsigned char, and then store them
 * all as initial weights in my NODE array.  The only thing to be
 * careful of is to make sure that a node with a non-zero count doesn't
 * get scaled down to 0. Nodes with values of 0 don't get codes.
 */
scale_counts(unsigned long *counts, NODE *nodes) {
  unsigned long max_count;
  int i;
  max_count = 0;
  for ( i = 0 ; i < 256 ; i++ )
    if ( counts[ i ] > max_count )
      max_count = counts[ i ];
  if ( max_count == 0 ) {
    counts[ 0 ] = 1;
    max_count = 1;
  }
  max_count = max_count / 255;
  max_count = max_count + 1;
  for ( i = 0 ; i < 256 ; i++ ) {
    nodes[ i ].count = (unsigned int) ( counts[ i ] / max_count );
    if ( nodes[ i ].count == 0 && counts[ i ] != 0 )
      nodes[ i ].count = 1;
  }
  nodes[ END_OF_STREAM ].count = 1;
}

/*
 * Since the Huffman tree is built as a decoding tree, there is
 * no simple way to get the encoding values for each symbol out of
 * it. This routine recursively walks through the tree, adding the
 * child bits to each code until it gets to a leaf.  When it gets
 * to a leaf, it stores the code value in the CODE element, and
 * returns.
 */
convert_tree_to_code(NODE *nodes, CODE *codes,
			  unsigned int code_so_far, int bits, int node) {
  if ( node <= END_OF_STREAM ) {
    codes[ node ].code = code_so_far;
    codes[ node ].code_bits = bits;
    return;
  }
  code_so_far <<= 1;
  bits++;
  convert_tree_to_code( nodes, codes, code_so_far, bits,
			nodes[ node ].child_0 );
  convert_tree_to_code( nodes, codes, code_so_far | 1,
			bits, nodes[ node ].child_1 );
}


/*
 * Once the tree gets built, and the CODE table is built, compressing
 * the data is a breeze.  Each byte is read in, and its corresponding
 * Huffman code is sent out.
 */
compress_data(CODE *codes) {
  int c;
  while((c=getc(tmp_file))!=EOF) {
    put_bits(codes[ c ].code, codes[ c ].code_bits );
  }
  put_bits(codes[END_OF_STREAM].code, codes[END_OF_STREAM].code_bits );
  flush();
}

/*
 * In order for the compressor to build the same model, I have to store
 * the symbol counts in the compressed file so the expander can read
 * them in.  In order to save space, I don't save all 256 symbols
 * unconditionally.  The format used to store counts looks like this:
 *
 *  start, stop, counts, start, stop, counts, ... 0
 *
 * This means that I store runs of counts, until all the non-zero
 * counts have been stored.  At this time the list is terminated by
 * storing a start value of 0.  Note that at least 1 run of counts has
 * to be stored, so even if the first start value is 0, I read it in.
 * It also means that even in an empty file that has no counts, I have
 * to pass at least one count.
 *
 * In order to efficiently use this format, I have to identify runs of
 * non-zero counts.  Because of the format used, I don't want to stop a
 * run because of just one or two zeros in the count stream.  So I have
 * to sit in a loop looking for strings of three or more zero values in
 * a row.
 *
 * This is simple in concept, but it ends up being one of the most
 * complicated routines in the whole program.  A routine that just
 * writes out 256 values without attempting to optimize would be much
 * simpler, but would hurt compression quite a bit on small files.
 *
 */
output_counts(NODE *nodes) {
  int first;
  int last;
  int next;
  int i;
  
  first = 0;
  while ( first < 255 && nodes[ first ].count == 0 )
    first++;
  /*
   * Each time I hit the start of the loop, I assume that first is the
   * number for a run of non-zero values.  The rest of the loop is *
   * concerned with finding the value for last, which is the end of
   * the * run, and the value of next, which is the start of the next
   * run.  * At the end of the loop, I assign next to first, so it
   * starts in on * the next run.
   */
  for ( ; first < 256 ; first = next ) {
    last = first + 1;
    for ( ; ; ) {
      for ( ; last < 256 ; last++ )
	if ( nodes[ last ].count == 0 )
	  break;
      last--;
      for ( next = last + 1; next < 256 ; next++ )
	if ( nodes[ next ].count != 0 )
	  break;
      if ( next > 255 )
	break;
      if ( ( next - last ) > 3 )
	break;
      last = next;
    };
    /*
     * Here is where I output first, last, and all the counts in
     * between.
     */
    putchar(first);
    putchar(last);
    for ( i = first ; i <= last ; i++ ) {
      putchar(nodes[i].count);
    }
  }
  putchar(0);
  fflush(stdout);
}


/*
 * When expanding, I have to read in the same set of counts.  This is
 * quite a bit easier that the process of writing them out, since no
 * decision making needs to be done.  All I do is read in first, check
 * to see if I am all done, and if not, read in last and a string of
 * counts.
 */
input_counts(NODE *nodes) {
  int first;
  int last;
  int i;
  int c;
  
  for ( i = 0 ; i < 256 ; i++ )
    nodes[ i ].count = 0;
  first = getchar();
  last = getchar();
  for ( ; ; ) {
    for ( i = first ; i <= last ; i++ ) {
      c = getc( stdin );
      nodes[ i ].count = (unsigned int)c;
    }
    first = getchar();
    if(first==0) break;
    last = getchar();
  }
  nodes[ END_OF_STREAM ].count = 1;
}

/*
 * Expanding compressed data is a little harder than the compression
 * phase.  As each new symbol is decoded, the tree is traversed,
 * starting at the root node, reading a bit in, and taking either the
 * child_0 or child_1 path.  Eventually, the tree winds down to a
 * leaf node, and the corresponding symbol is output.  If the symbol
 * is the END_OF_STREAM symbol, it doesn't get written out, and
 * instead the whole process terminates.
 */
expand_data(NODE *nodes, int root_node) {
  int node;
  
  for ( ; ; ) {
    node = root_node;
    do {
      if(get_bit()) {
	node = nodes[ node ].child_1;
      }
      else {
	node = nodes[ node ].child_0;
      }
    } while ( node > END_OF_STREAM );
    if ( node == END_OF_STREAM )
      break;
    putchar(node);
  }
}

/*
 * Building the Huffman tree is fairly simple.  All of the active nodes
 * are scanned in order to locate the two nodes with the minimum
 * weights. These two weights are added together and assigned to a new
 * node. The new node makes the two minimum nodes into its 0 child
 * and 1 child. The two minimum nodes are then marked as inactive.
 * This process repeats until their is only one node left, which is the
 * root node.  The tree is done, and the root node is passed back
 * to the calling routine.
 *
 * Node 513 is used here to arbitratily provide a node with a guaranteed
 * maximum value.  It starts off being min_1 and min_2.  After all active
 * nodes have been scanned, I can tell if there is only one active node
 * left by checking to see if min_1 is still 513.
 */
int build_tree( NODE *nodes ) {
  int next_free;
  int i;
  int min_1;
  int min_2;
  
  nodes[ 513 ].count = 0xffff;
  for ( next_free = END_OF_STREAM + 1 ; ; next_free++ ) {
    min_1 = 513;
    min_2 = 513;
    for ( i = 0 ; i < next_free ; i++ )
      if ( nodes[ i ].count != 0 ) {
	if ( nodes[ i ].count < nodes[ min_1 ].count ) {
	  min_2 = min_1;
	  min_1 = i;
	} else if ( nodes[ i ].count < nodes[ min_2 ].count )
	  min_2 = i;
      }
    if ( min_2 == 513 )
      break;
    nodes[ next_free ].count = nodes[ min_1 ].count
      + nodes[ min_2 ].count;
    nodes[ min_1 ].count = 0;
    nodes[ min_2 ].count = 0;
    nodes[ next_free ].child_0 = min_1;
    nodes[ next_free ].child_1 = min_2;
  }
  next_free--;
  return next_free;
}

