
.PHONY: db server

db:
	cd db && make build-db -B
server:
	cd web && make && ./server	
