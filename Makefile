
.PHONY: db server

music_lib := musikk
music_db  := musikk.sqlite

music_lib_path := $(shell pwd)/$(music_lib)
music_db_path  := $(shell pwd)/$(music_db)

db:
	make -C db build-db
	cd db && ./build-db $(music_lib_path) $(music_db_path)

server:
	make -C web server
	cd web && ./server $(music_db_path)
