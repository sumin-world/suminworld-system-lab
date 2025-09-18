CC := gcc
CFLAGS := -O2 -Wall -Wextra -pedantic

all: signal_demo

signal_demo: signal_demo.c
	$(CC) $(CFLAGS) -o $@ $<

run: signal_demo
	./signal_demo

test: signal_demo
	chmod +x test.sh
	./test.sh

clean:
	rm -f signal_demo *.o

.PHONY: all run test clean