.POSIX:

SH = /bin/sh
CC = cc
RM = rm -rf

CFLAGS = -g -std=c89 -pedantic -Werror -Wall -Wextra -Wunreachable-code

clopts: main.o libclopts.o
	$(CC) $(CFLAGS) -o clopts main.o libclopts.o

main.o: main.c clopts.h
	$(CC) $(CFLAGS) -o main.o -c main.c

libclopts.o: clopts.c clopts.h
	$(CC) $(CFLAGS) -o libclopts.o -c clopts.c

.PHONY: clean
clean:
	$(RM) clopts main.o libclopts.o

