
# Assignment: CS 111 p4c - Spring 2017

CC = gcc
CFLAGS = -g -lm -lmraa
CFLAGS_tls = $(CFLAGS) -lssl -lcrypto
SOURCEFILES = lab4c_tcp.c lab4c_tls.c

OBJS = lab2_tcp.o lab2_tls.o


build: $(SOURCEFILES)
	$(CC) lab4c_tcp.c -o lab4c_tcp $(CFLAGS)
	$(CC) lab4c_tls.c -o lab4c_tls $(CFLAGS_tls)

clean:
	-rm -f $(OBJS) lab4c_tcp lab4c_tls lab4c-867530900.tar.gz *~

dist: 
	tar -czvf lab4c-867530900.tar.gz $(SOURCEFILES) Makefile README