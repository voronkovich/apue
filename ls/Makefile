.PHONY: clean, mrproper
CC = gcc
CFLAGS = -g -Wall

all: ls

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

ls: ls.o
	$(CC) $(CFLAGS) -o $@ $+

clean:
	rm -f *.o core.*

mrproper: clean
	rm -f ls
