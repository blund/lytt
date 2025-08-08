
.PHONY: template server
template:
	gcc template.c -lsqlite3 -o template && ./template && rm template

server: template
	gcc server.c -lsqlite3 -lmicrohttpd -o server && ./server
