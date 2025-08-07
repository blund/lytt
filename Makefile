
build-db:
	rm musikk.sqlite
	touch musikk.sqlite
	gcc musikk.c db.c files.c \
		-lm -lsqlite3 -I./external/include -L./lib/lib -ltag_c \
		-o build-db
	./build-db musikk

prepare:
	cd external/taglib && cmake -DCMAKE_INSTALL_PREFIX=../. -DWITH_SHORTEN=OFF -DWITH_TRUEAUDIO=OFF -DWITH_APE=OFF -DWITH_ASF=OFF -DWITH_DSF=OFF -DWITH_MP4=OFF && cmake --build . --target install


server:
	cd web && make && ./server	
