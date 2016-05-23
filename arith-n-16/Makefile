CFLAGS	= -g
EXE =

arith-n-16-c:	main-c.c arith-n.c errhand.c bitio.c
		gcc  $(CFLAGS) main-c.c arith-n.c  errhand.c bitio.c -o arith-n-16-c
EXE += arith-n-16-c

arith-n-16-d:	main-d.c arith-n.c errhand.c bitio.c
		gcc  $(CFLAGS) main-d.c arith-n.c  errhand.c bitio.c -o arith-n-16-d
EXE += arith-n-16-d

all:	$(EXE)

clean:
	rm -f $(EXE)

slides:
