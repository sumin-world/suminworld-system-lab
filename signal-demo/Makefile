CC := gcc
CFLAGS := -O2 -Wall -Wextra -pedantic -Wno-unused-result

all: signal_demo

signal_demo: signal_demo.c
	$(CC) $(CFLAGS) -o $@ $<

debug: signal_demo.c
	$(CC) -g -O0 -Wall -Wextra -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -o signal_demo $<

run: signal_demo
	./signal_demo

test: signal_demo
	chmod +x test.sh
	./test.sh

clean:
	rm -f signal_demo *.o

.PHONY: all debug run test clean
