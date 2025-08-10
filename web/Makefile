
.PHONY: template server
template:
	gcc template.c helpers.c -lsqlite3 -lmicrohttpd -o template && ./template && rm template

server: template
	gcc server.c helpers.c -lsqlite3 -lmicrohttpd -o server && ./server
