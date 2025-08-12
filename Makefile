
.PHONY: db server

music_lib := musikk
music_db  := musikk.sqlite

music_lib_path := $(shell pwd)/$(music_lib)
music_db_path  := $(shell pwd)/$(music_db)

db:
	rm -f $(music_db_path)
	make -C db build-db
	cd db && ./build-db $(music_lib_path) $(music_db_path)

server:
	make -C web server

run: server
	cd web && ./server $(music_db_path)

debug: server
	cd web && gdb --args ./server $(music_db_path)

deploy: server
	cd web && nohup ./server $(music_db_path) > output.log 2>&1 &
