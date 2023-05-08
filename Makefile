CC = gcc
CFLAGS = -Wall -Wextra -lm

build: server subscriber

subscriber: subscriber.o common.o
	$(CC) -o $@ $^ $(CFLAGS)

subscriber.o: subscriber.c common.h helpers.h
	$(CC) -c -o $@ $< $(CFLAGS)

server: server.o common.o
	$(CC) -o $@ $^ $(CFLAGS)

server.o: server.c helpers.h common.h
	$(CC) -c -o $@ $< $(CFLAGS)

common.o: common.c common.h

clean: 
	rm *.o server subscriber

zip: 
	zip -u "Popa Rares Teodor".zip *.c *.h Makefile