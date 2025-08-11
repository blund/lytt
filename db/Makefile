
.PHONY: build-db

build-db:
	gcc build-db.c db.c files.c \
		-lm -lsqlite3 -I./external/include -L./lib/lib -ltag_c \
		-o build-db

prepare:
	cd external/taglib && cmake -DCMAKE_INSTALL_PREFIX=../. -DWITH_SHORTEN=OFF -DWITH_TRUEAUDIO=OFF -DWITH_APE=OFF -DWITH_ASF=OFF -DWITH_DSF=OFF -DWITH_MP4=OFF && cmake --build . --target install
