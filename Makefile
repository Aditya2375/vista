CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=gnu11 -Wall -Wextra -Werror -Isrc

all: build/libvista.a build/demo

build/libvista.a: src/surface.c src/widget.c src/run.c src/vista.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c src/surface.c -o build/surface.o
	$(CC) $(CFLAGS) -c src/widget.c -o build/widget.o
	$(CC) $(CFLAGS) -c src/run.c -o build/run.o
	ar rcs $@ build/surface.o build/widget.o build/run.o

build/demo: examples/demo.c build/libvista.a
	$(CC) $(CFLAGS) -Iexamples examples/demo.c build/libvista.a -o $@

check: all
	./tests/run-tests.sh

clean:
	rm -rf build

.PHONY: all check clean
