CFLAGS	= -g
EXE =

arith-n-c:	main-c.c arith-n.c errhand.c bitio.c
		gcc  $(CFLAGS) main-c.c arith-n.c  errhand.c bitio.c -o arith-n-c -pthread
EXE += arith-n-c

arith-n-d:	main-d.c arith-n.c errhand.c bitio.c
		gcc  $(CFLAGS) main-d.c arith-n.c  errhand.c bitio.c -o arith-n-d -pthread 
EXE += arith-n-d

all:	$(EXE)

clean:
	rm -f $(EXE)

slides:
