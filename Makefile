CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I/usr/include -Isrc/common
LDFLAGS = -lclang

SRC = $(wildcard src/*.c) $(wildcard src/common/*.c)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
BIN = rcc

all: build $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build build/common

clean:
	rm -rf build $(BIN)

run: all
	./$(BIN) $(ARGS)

.PHONY: all clean run
