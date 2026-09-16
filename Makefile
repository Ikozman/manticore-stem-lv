CC       ?= cc
MINGW_CC ?= x86_64-w64-mingw32-gcc
CFLAGS   ?= -O2 -Wall -Wextra -std=c99

SOURCES = src/stem_lv.c src/latvian_stemmer.c
HEADERS = src/latvian_stemmer.h

.PHONY: all linux windows test clean

all: linux

linux: build/stem_lv.so

windows: build/stem_lv.dll

build/stem_lv.so: $(SOURCES) $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -shared -o $@ $(SOURCES)

build/stem_lv.dll: $(SOURCES) $(HEADERS)
	@mkdir -p build
	$(MINGW_CC) $(CFLAGS) -shared -static-libgcc -o $@ $(SOURCES)

build/test_stemmer: tests/test_stemmer.c $(SOURCES) $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/test_stemmer.c $(SOURCES)

test: build/test_stemmer
	./build/test_stemmer

clean:
	rm -rf build
