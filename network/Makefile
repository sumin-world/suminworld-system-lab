CC=gcc
CFLAGS=-O2 -Wall -Wextra

all: server client select_server

server: echo_server/echo_server.c
	$(CC) $(CFLAGS) -o echo_server/server echo_server/echo_server.c

client: echo_client/echo_client.c
	$(CC) $(CFLAGS) -o echo_client/client echo_client/echo_client.c

select_server: echo_server_select/echo_server_select.c
	$(CC) $(CFLAGS) -o echo_server_select/server_select echo_server_select/echo_server_select.c

clean:
	rm -f echo_server/server echo_client/client echo_server_select/server_select
