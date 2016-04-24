/*
 * PPM text predictive coding.
 * gse. 1999.
 */

/*
  Respecto de la versi'on anterior, una ventana deslizante es definida con el
  objetivo de reducir el consumo de memoria.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codec.h"

#define UCHAR  unsigned char
#define SYMBOL unsigned char
#define ORDER  unsigned char
#define CODE   unsigned char

#ifdef _HASH_INFO_
int num_colisiones=0;
int num_tablas=0;
#endif

#ifdef _INFO_PREDICTION_
FILE *fo;
#endif

/**************************************************/
/* M A N E J O   D E   L A   T A B L A   H A S H  */
/**************************************************/

/* La estructura de datos utilizada para manejar los contextos es la siguiente:
   Tenemos una tabla hash, en la que cada entrada es un puntero a un contexto.
   Cada contexto se compone de 4 componentes: (1) el orden del contexto,
   (2) un puntero a una lista enlazada que almacena el contexto mediante
   pares s'imbolo/recuento y (3) el array de caracteres que almacenan el
   contexto. Gra'ficamente:

  hash_table       CONTEXT_TABLE
   +------+  +-------+------+-----------+
   | ptr -|->| order | list | context[] |
   +------+  +-------+---|--+-----------+
   |      |              v
   :      :  +--------+-------+------+
   :      :  | symbol | count | next-|-> ...
             +--------+-------+------+
                    SYMBOL_NODE

   La tabla hash implementada permite la ocurrencia de colisiones. Cuando
   una ocurre (dos o m'as contextos comparten la misma posici'on dentro de
   la tabla) se realiza una b'usqueda secuencial a partir de la direcci'on
   de colisi'on. El n'umero de b'usquedas secuenciales m'aximo permitido se
   declara en la constante MAX_COLLISIONS. La tabla hash es est'atica, ya que
   no se permite que el n'umero de contextos aumente si la tabla est'a
   demasiado llena (esto ser'ia lo ideal, pero por ahora no se hace). El
   n'umero m'aximo de contextos permitidos es MAX_CONTEXTS. */

struct SYMBOL_NODE {
  UCHAR symbol;
  UCHAR count;
  struct SYMBOL_NODE *next;
};

typedef struct {
  UCHAR order;
#ifdef _1_
  unsigned long time;      /* Instante de la 'ultima actualizaci'on */
#endif
  struct SYMBOL_NODE *list;
} CONTEXT_TABLE;

CONTEXT_TABLE *hash_table[65537][32];

/* En esta tabla se indica si un s'imbolo ya ha sido visitado en contextos
   de orden superior durante la b'usqueda del s'imbolo a codificar. As'i,
   se acelera el proceso de b'usqueda. */
unsigned long file_position=0;
unsigned long visited[256];
#ifdef _1_
#define WINDOW_SIZE 65536
#endif

#ifdef _MEMORY_INFO_
int num_contextos=0;
long memory_used=0;
#define MALLOC(a) malloc(a); memory_used += a;
#else
#define MALLOC(a) malloc(a)
#endif

/* Inicializa la tabla del contexto de orden 0 */
void initialize_0_order_context_table() {
  int i;
  struct SYMBOL_NODE *node;
  hash_table[0][0]=MALLOC(sizeof(CONTEXT_TABLE));
  hash_table[0][0]->order=0;
  node=hash_table[0][0]->list=MALLOC(sizeof(struct SYMBOL_NODE));
  for(i=1;i<256;i++) {
    node->next=MALLOC(sizeof(struct SYMBOL_NODE));
    node=node->next;
    node->symbol=(unsigned char)(i/*+32*/);
    node->count=0;
  }
}

#ifdef _1_
void free_list(struct SYMBOL_NODE *s) {
  struct SYMBOL_NODE *old;
  while(s!=NULL) {
    old=s;
    s=s->next;
    free(old);
  }
}
#endif

inline void create_context(int table, int pos, char *context, UCHAR order) {
  hash_table[table][pos]=MALLOC(sizeof(CONTEXT_TABLE)+order);
  strncpy((char *)hash_table[table][pos]+sizeof(CONTEXT_TABLE),context,order);
  hash_table[table][pos]->list=NULL;
  hash_table[table][pos]->order=order;
#ifdef _1_
  hash_table[table][pos]->time=file_position;
#endif
}

/* Busca el contexto "context" de orden "order" en la tabla hash. Si lo
   encuentra devuelve su posici'on mediante un puntero, y sino, se crea
   una entrada en la tabla y la estructura CONTEXT_TABLE asociada al
   contexto. Ahora si el contexto no ha sido usado al menos WINDOW_SIZE
   s'imbolos, el contexto se libera y se reutiliza. */
CONTEXT_TABLE *locate(char *context, UCHAR order) {
  int table;
  unsigned long addr;
  for(table=0;table<32;table++) {
    int i;
    addr=0;
    for(i=0;i<order;i++) addr ^= context[i]<<(i*(table+1));
    addr %= 65537;
    if(hash_table[addr][table]==NULL) {
      create_context(addr,table,context,order);            /* Creamos un marco de contexto */
#ifdef _HASH_INFO_
        num_contextos++;
	if(num_tablas<table) num_tablas=table;
#endif
      return hash_table[addr][table];           /* Retornamos su direcci'on */
    }
    if(hash_table[addr][table]->order!=order) {
#ifdef _HASH_INFO_
      num_colisiones++;
#endif
      continue;
    }
    if(strncmp((char *)hash_table[addr][table]+sizeof(CONTEXT_TABLE),context,order)) {
#ifdef _HASH_INFO_
      num_colisiones++;
#endif
      continue;
    }
    return hash_table[addr][table];
  }
  fprintf(stderr,"tpc-ppm: tablas hash llenas\n");
  exit(1);
}

/*********************************************************************/
/* M A N E J O   D E   L A S   L I S T A S   D E   C O N T E X T O S */
/*********************************************************************/

/* Escala un contexto dividiendo todos los recuentos entre 2. */
void scale_context(CONTEXT_TABLE *p) {
  struct SYMBOL_NODE *s=p->list;
  while(s!=NULL) {
    s->count >>= 1;
    s=s->next;
  }
}

/* M'aximo recuento acumulado permitido para un s'imbolo. Este valor es
   importante para el nivel entr'opico de salida alcanzable debido a que
   controla la frecuencia de escalados en los contextos. */
#define MAX_COUNT     255

UCHAR search_update_context(UCHAR symbol, char *context, UCHAR order) {
  CONTEXT_TABLE *context_table=locate(context,order);
  UCHAR code=0;
  int found=0;
  struct SYMBOL_NODE *s,*r;
  if(context_table->list==NULL) { /* Lista vac'ia ? */
    context_table->list=MALLOC(sizeof(struct SYMBOL_NODE));
    context_table->list->symbol=symbol;
    context_table->list->count=0;
    context_table->list->next=NULL;
  } else {
    s=context_table->list;
#ifdef _INFO_PREDICTION_
    putc(s->symbol,fo);
#endif
    while(s!=NULL) {
      if(visited[s->symbol]!=file_position) {
        visited[s->symbol]=file_position;
        if(s->symbol==symbol) { found=1; break; }
        code++;
      }
      r=s; s=s->next;
    }
    if(found) {
      if(s->count==MAX_COUNT) {
#ifdef _SCALING_INFO_
        fprintf(stderr,"S-%d ",order);
#endif
        scale_context(context_table);
      }
#ifndef _NO_COUNTS_
      s->count++;
#endif
      if(s!=context_table->list) { /* Si "s" no es el primer nodo de la lista */
        struct SYMBOL_NODE *h,*g;
        if(r->count<=s->count) { /* Si la lista no est'a ordenada */
          h=context_table->list;
          while(h->count>s->count) { g=h; h=h->next; }
          if(h!=context_table->list) { /* Si la nueva posici'on no es la cabeza */
            g->next=s;
          } else { /* pero si s'i lo es */
            context_table->list=s;
          }
          r->next=s->next;
          s->next=h;
        }
      }
    } else { /* Si el nodo no est'a en la lista, lo a~nadimos por el final */
      struct SYMBOL_NODE *h,*g;
      s=MALLOC(sizeof(struct SYMBOL_NODE));
      s->symbol=symbol;
      s->count=0;
      h=context_table->list;
      while(h->count) {
        g=h; h=h->next;
        if(h==NULL) break;
      }
      if(h!=context_table->list) {
        g->next=s;
        s->next=h;
      } else {
        s->next=h;
        context_table->list=s;
      }
    }
  }
  return code;
}

/* Creates a new context "c" with the symbol "symbol". */
void CreateContext(CONTEXT_TABLE *c, UCHAR symbol) {
  c->list=MALLOC(sizeof(struct SYMBOL_NODE));
  c->list->symbol=symbol;
  c->list->count=0;
  c->list->next=NULL;
} 

/* Update the symbol's count of a symbol in a context table. The symbol must 
   exits in the context table.
*/
void UpdateSymbol(CONTEXT_TABLE *ct, SYMBOL symbol) {
  struct SYMBOL_NODE *s=ct->list,*r;
  while(symbol!=s->symbol) {r=s; s=s->next;} /* Searching */
  if(s->count==MAX_COUNT) /*ScaleContext*/scale_context(ct);
  s->count++;
  if(s!=ct->list) { /* If the updated symbol isn't on the top of the list */
    struct SYMBOL_NODE *h,*g;
    if(r->count<=s->count) { /* If the list isn't sorted */
      h=ct->list; /* Looking for the new position in the list */
      while(h->count>s->count) { g=h; h=h->next; }
      if(h!=ct->list) g->next=s; else ct->list=s;
      r->next=s->next;
      s->next=h;
    }
  }
}
  
/* Add a new symbol to a context table.
*/
void AddSymbolToContext(CONTEXT_TABLE *ct, SYMBOL symbol) {
  if(ct->list==NULL) CreateContext(ct,symbol);
  else { /* Insert the symbol at the begin of the 0-count sub-list */
    struct SYMBOL_NODE *s=ct->list,*r,*new;
    if(s->count) {
      while((s!=NULL) && (s->count)) {
        r=s; s=s->next;
      }
      new=MALLOC(sizeof(struct SYMBOL_NODE));
      new->symbol=symbol;
      new->count=0;
      new->next=s;
      r->next=new;
    } else {
      new=MALLOC(sizeof(struct SYMBOL_NODE));
      new->symbol=symbol;
      new->count=0;
      new->next=s;
      ct->list=new;
    }
  }
}

/* Retorna el s'imbolo colocado en la posici'on "code" en la tabla de
   contexto "ct". Si no se encuentra, devuelve -1. En cualquier caso,
   "code" se decrementa en cada b'usqueda.
*/
int FindSymbol(int *code, CONTEXT_TABLE *ct) {
  SYMBOL symbol;
  int found=0;
  struct SYMBOL_NODE *s=ct->list;
  while(s!=NULL) { /* loops inside of a context table */
    if(visited[s->symbol]!=file_position) {
      if((*code)==0) {symbol=s->symbol; found=1; break;}
      visited[s->symbol]=file_position;
      (*code)--;
    }
    s=s->next;
  }
  if(found) return symbol; else return -1;
}

#ifdef _INFO_
char filtro(char ch) {
  if(ch<32) ch='·';
  return ch;
}

void print_context_table(char *context, UCHAR order) {
  CONTEXT_TABLE *ct=locate(context,order);
  struct SYMBOL_NODE *s=ct->list;
  int c=0;
  while(s!=NULL) {
    fprintf(stderr,"%c,%d ",filtro(s->symbol),s->count);
    s=s->next;
    if(c>8) break;
    c++;
  }
}
#endif

int i;
int symbol;       /* S'imbolo a codificar */
int code;         /* Error de predicci'on */
int order;        /* Orden actual de predicci'on */
ORDER max_order;    /* M'aximo orden permitido */
SYMBOL *context;    /* Contexto actual */

void encode_stream(int argc, char *argv[]) {
  max_order=atoi(argv[2]);
  fprintf(stderr,"tpc-ppm: prediction order: %d\n",max_order);
  context=MALLOC(sizeof(UCHAR)*max_order);
  if(!context) {
    fprintf(stderr,".");
    fprintf(stderr,"tpc-ppm: sin memoria para el vector contexto\n");
    exit(1);
  }
  
#ifdef _INFO_PREDICTION_
  /* Output error-prediction file */
  fo=fopen("error","wb");
  if(!fo) {
    fprintf(stderr,"Unable to open \"error\"\n");
    exit(-1);
  }
#endif
  
  /* Inicializamos el vector contexto */
  for(i=max_order-1;i>=0;i--) {
    context[i]=getchar();
    putchar(context[i]);
    file_position++;
  }
  
  initialize_0_order_context_table();
  
  fprintf(stderr,"tpc-ppm: coding ...\n");
  symbol=getchar(); file_position++;
  while(symbol!=EOF) {
    code=0;
    order=max_order;
    for(;;) {
      code += search_update_context(symbol,context,order);
      if(visited[symbol]==file_position) break;
      order--;
    }
    putchar(code);
    
#ifdef _INFO_
    {
      int i;
      fprintf(stderr,"%3d(%c) %3d %2d |",symbol,filtro(symbol),code,order);
      for(i=max_order-1;i>=0;i--) {
	fprintf(stderr,"%c",filtro(context[i]));
      }
      fprintf(stderr,"|");
      for(i=0;i<=max_order;i++) {
	print_context_table(context,i);
	fprintf(stderr,"|");
      }
      fprintf(stderr,"\n");
    }
#endif
#ifdef _MEMORY_INFO_
    {
      int counter;
      if(!(counter++%256)) {
	fprintf(stderr,"tpc-ppm: memoria: %ld\n",memory_used);
      }
    }
#endif
#ifdef _MEMORY_INFO_
    {
      int counter;
      if(!(counter++%256)) {
	fprintf(stderr,"tpc-ppm: contextos: %d\n",num_contextos);
      }
    }
#endif
    /* Actualizamos la cadena con el nuevo contexto */
    for(i=max_order-1;i>0;i--) context[i]=context[i-1];
    context[0]=symbol;
    symbol=getchar();
    file_position++;
    if(!file_position) memset(visited,1,256*4);
  }
  
#ifdef _HASH_INFO_
  fprintf(stderr,"tpc-ppm: n'unmero de contextos: %d\n",num_contextos);
  fprintf(stderr,"pmi: N'umero de tablas hash usadas: %d\n",num_tablas);
  fprintf(stderr,"pmi: N'umero de colisiones: %d\n",num_colisiones);
#endif
#ifdef _MEMORY_INFO_
  fprintf(stderr,"tpc-ppc: memoria consumida: %ld bytes\n",memory_used);
#endif
  fprintf(stderr,"tpc-ppm: done\n");
  
}

void decode_stream(int argc, char *argv[]) {
  max_order=atoi(argv[2]);
  fprintf(stderr,"tpc-ppm: prediction order: %d\n",max_order);
  context=MALLOC(sizeof(UCHAR)*max_order);
  if(!context) {
    fprintf(stderr,".");
    fprintf(stderr,"tpc-ppm: sin memoria para el vector contexto\n");
    exit(1);
  }
  
  /* Inicializamos el vector contexto */
  for(i=max_order-1;i>=0;i--) {
    context[i]=getchar();
    putchar(context[i]);
    file_position++;
  }
  
  initialize_0_order_context_table();
  
  fprintf(stderr,"tpc-ppm: decoding ...\n");
  code=getchar(); file_position++;
  while(code!=EOF) {
    order=max_order;
    for(;;) {
      CONTEXT_TABLE *ct=locate(context,order);
      symbol=FindSymbol(&code,ct);
      if(symbol!=-1) {
	UpdateSymbol(ct,symbol);
	break;
      }
      order--;
    }
    for(i=order+1;i<=max_order;i++) {
      CONTEXT_TABLE *ct=locate(context,i);
      AddSymbolToContext(ct,symbol);
    }
    putchar(symbol);
#ifdef _INFO_
    {
      int i;
      fprintf(stderr,"%3d(%c) %3d %2d |",symbol,filtro(symbol),code,order);
      for(i=0;i<=max_order;i++) {
	print_context_table(context,i);
	fprintf(stderr,"|");
      }
      for(i=max_order-1;i>=0;i--) {
	fprintf(stderr,"%c",filtro(context[i]));
      }
      fprintf(stderr,"|\n");
    }
#endif
    /* Actualizamos la cadena con el nuevo contexto */
    for(i=max_order-1;i>0;i--) context[i]=context[i-1];
    context[0]=symbol;
    code=getchar();
    file_position++;
    if(!file_position) memset(visited,1,256*4);
  }
#ifdef _HASH_INFO_
  fprintf(stderr,"tpc-ppm: n'unmero de contextos: %d\n",num_contextos);
  fprintf(stderr,"pmi: N'umero de tablas hash usadas: %d\n",num_tablas);
  fprintf(stderr,"pmi: N'umero de colisiones: %d\n",num_colisiones);
#endif
#ifdef _MEMORY_INFO_
  fprintf(stderr,"tpc-ppc: memoria consumida: %ld bytes\n",memory_used);
#endif
  fprintf(stderr,"tpc-ppm: done\n");
}
