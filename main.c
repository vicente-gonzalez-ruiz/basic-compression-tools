#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "codec.h"

int main(int argc, char *argv[]) {
  clock_t ticks;
  if(argc<=1) {
    fprintf(stderr,"%s: e|d [options] < stdin > stdout\n", argv[0]);
    exit(1);
  }
  if(argv[1][0]=='e') {
    fprintf(stderr,"%s: encoding ...\n", argv[0]);
    ticks = clock();
    encode_stream(argc, argv);
    ticks = clock()-ticks;
  } else {
    fprintf(stderr,"%s: decoding ...\n", argv[0]);
    ticks = clock();
    decode_stream(argc, argv);
    ticks = clock()-ticks;
  }
  float time= (float)ticks/CLOCKS_PER_SEC;
  fprintf(stderr,"%s: run time = %f seconds\n", argv[0], time);
}

