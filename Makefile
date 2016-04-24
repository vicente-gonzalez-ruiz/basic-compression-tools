#CFLAGS	= -O -I .
CFLAGS = -g -I .

EXE =

rle:		main.o rle.c
		gcc $(CFLAGS) $^ -o $@
EXE += rle

bwt:		bwt.cpp
		g++ $(CFLAGS) $^ -o $@
EXE += bwt

unbwt:		unbwt.cpp
		g++ $(CFLAGS) $^ -o $@
EXE += unbwt

lzss:		main.o bitio.o lzss.c
		gcc $(CFLAGS) $^ -o $@
EXE += lzss

lzw15v:		main.o bitio.o lzw15v.c
		gcc $(CFLAGS) $^ -o $@
EXE += lzw15v

huff_s0:	main.o bitio.o huff.c
		gcc $(CFLAGS) $^ -o $@
EXE += huff_s0

huff_a0:	main.o bitio.o huff_a0.c
		gcc $(CFLAGS) $^ -o $@
EXE += huff_a0

unary:		main.o bitio.o model_a0.o unary.c
		gcc $(CFLAGS) $^ -o $@
EXE += unary

rice:		main.o bitio.o model_a0.o rice.c
		gcc $(CFLAGS) $^ -o $@
EXE += rice

golomb:	main.o bitio.o model_a0.o golomb.c
		gcc $(CFLAGS) $^ -o $@ -lm
EXE += golomb

arith_a0:	main.o bitio.o model_a0.o arith.c
		gcc $(CFLAGS) $^ -o $@
EXE += arith_a0

arith-n-c:
		make -C arith-n arith-n-c
		cp arith-n/arith-n-c .
EXE += arith-n-c

arith-n-d:
		make -C arith-n arith-n-d
		cp arith-n/arith-n-d .
EXE += arith-n-d

mtf:		main.o mtf.c
		gcc $(CFLAGS) $^ -o $@
EXE += mtf

tpt:		main.o tpt.c
		gcc $(CFLAGS) $^ -o $@
EXE += tpt

snr:	snr.c
	gcc $< -o $@ -lm -lfftw3
EXE += snr

EXE += vix2raw

all:	$(EXE)

clean:
	rm -f $(EXE) *.o
	make -C arith-n clean

/tmp/compression-utils.tar.gz:
	rm -rf /tmp/compression-utils
	svn export . /tmp/compression-utils
	tar --create --file=/tmp/compression-utils.tar -C /tmp compression-utils
	gzip -9 /tmp/compression-utils.tar

publish:	/tmp/compression-utils.tar.gz
	scp /tmp/compression-utils.tar.gz www.ace.ual.es:public_html/doctorado
	rm /tmp/compression-utils.tar.gz
