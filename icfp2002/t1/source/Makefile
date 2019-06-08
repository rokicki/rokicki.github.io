CFLAGS = -g -Wall -O
CC = gcc
SRC = README lessobliv.c client.c util.c dist.c ip.c pqueue.c \
      client.h pqueue.h util.h ip.h dist.h quicksim.c Makefile buildme

all: server human obliv lessobliv

client.o:	client.h pqueue.h util.h ip.h dist.h

human:	human.o client.o util.o
	$(CC) -o human human.o client.o util.o -lm

obliv:	obliv.o client.o util.o dist.o
	$(CC) -o obliv obliv.o client.o util.o dist.o -lm

lessobliv:	lessobliv.o client.o util.o dist.o ip.o pqueue.o quicksim.o
	$(CC) -o lessobliv lessobliv.o client.o util.o dist.o ip.o pqueue.o quicksim.o -lm -static

server:	board.h

server: server.o readboard.o moves.o util.o log.o tcp.o
	$(CC) -o server server.o readboard.o moves.o util.o log.o tcp.o -lm

clean:
	rm -f *.o core

tidy:	clean
	rm -f *~
	cd distro; make clean

dist:
	-rm -rf distro
	-mkdir distro
	-mkdir distro/source
	cp -f $(SRC) distro/source
	cd distro/source; make lessobliv clean
	mv distro/source/lessobliv distro/runme
	ln distro/source/buildme distro/buildme
	(cd distro; tar -cf - .) | gzip > distro.tar.gz
	wc -c distro.tar.gz
	md5sum distro.tar.gz
