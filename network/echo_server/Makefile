# Makefile — echo servers (single & multi-client via select)

CC      ?= gcc
CSTD    ?= -std=c11
WARN    ?= -Wall -Wextra -Wshadow -Wformat=2 -Wcast-qual -Wpointer-arith
OPT     ?= -O2
DBG     ?= -g3
CFLAGS  ?= $(CSTD) $(WARN) $(OPT) $(DBG)
LDFLAGS ?=
PORT    ?= 8080

SRC_SINGLE        := echo_server.c
SRC_MULTI         := multi_echo_server.c
SRC_MULTI_SELECT  := multi_echo_server_select.c

BIN_SINGLE        := echo_server
BIN_MULTI         := multi_echo_server
BIN_MULTI_SELECT  := multi_echo_server_select

.PHONY: all clean debug asan run run-multi run-select

all: $(BIN_SINGLE) $(BIN_MULTI) $(BIN_MULTI_SELECT)

$(BIN_SINGLE): $(SRC_SINGLE)
	$(CC) $(CFLAGS) -DPORT=$(PORT) $< -o $@ $(LDFLAGS)

$(BIN_MULTI): $(SRC_MULTI)
	$(CC) $(CFLAGS) -DPORT=$(PORT) $< -o $@ $(LDFLAGS)

$(BIN_MULTI_SELECT): $(SRC_MULTI_SELECT)
	$(CC) $(CFLAGS) -DPORT=$(PORT) $< -o $@ $(LDFLAGS)

# Debug 빌드(최적화 끄고 디버그 심벌 최대)
debug: CFLAGS := $(CSTD) $(WARN) -O0 -g3
debug: clean all

# Address/UB Sanitizer 빌드
asan: CFLAGS := $(CSTD) $(WARN) -O1 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
asan: LDFLAGS := -fsanitize=address,undefined
asan: clean all

# 편의 실행 타겟
run: $(BIN_SINGLE)
	./$(BIN_SINGLE)

run-multi: $(BIN_MULTI)
	./$(BIN_MULTI)

run-select: $(BIN_MULTI_SELECT)
	./$(BIN_MULTI_SELECT)

clean:
	rm -f $(BIN_SINGLE) $(BIN_MULTI) $(BIN_MULTI_SELECT) *.o
