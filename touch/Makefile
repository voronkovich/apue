.PHONY: clean, mrproper
CC = gcc
CFLAGS = -g -Wall

all: touch

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

touch: touch.o
	$(CC) $(CFLAGS) -o $@ $+

clean:
	rm -f *.o core.*

mrproper: clean
	rm -f touch
