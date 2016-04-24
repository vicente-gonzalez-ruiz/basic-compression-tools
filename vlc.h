void   init_encoder();
void   init_decoder();
void   encode_index(int index, unsigned short *cum_counts);
int    decode_index(unsigned short *cum_counts);
void finish_encoder();
void finish_decoder();
