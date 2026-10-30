CC      ?= cc
CFLAGS  ?= -O2 -g
# required flags survive both the environment and `make CFLAGS=...`;
# callers add their own through EXTRA
override CFLAGS += -std=gnu11 -Wall -Wextra -Werror -Isrc $(EXTRA)

all: build/libvista.a build/demo

build/libvista.a: src/surface.c src/widget.c src/run.c src/keys.c src/vista.h src/keys.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/surface.c -o build/surface.o
	$(CC) $(CFLAGS) -c src/widget.c -o build/widget.o
	$(CC) $(CFLAGS) -c src/run.c -o build/run.o
	$(CC) $(CFLAGS) -c src/keys.c -o build/keys.o
	ar rcs $@ build/surface.o build/widget.o build/run.o build/keys.o

build/demo: examples/demo.c build/libvista.a
	$(CC) $(CFLAGS) -Iexamples examples/demo.c build/libvista.a -o $@

check: all
	./tests/run-tests.sh

test: check

clean:
	rm -rf build

.PHONY: all check test clean
