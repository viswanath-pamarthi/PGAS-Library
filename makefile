
libpgas.a:	pgas.c pgas.h
	mpicc -g -c pgas.c -o pgas.o
	ar rf libpgas.a pgas.o
	
p5test: p5test.c  libpgas.a
	mpicc -g -o p5test p5test.c -L. -lpgas  -lpthread

clean:
	rm -rf *.o
