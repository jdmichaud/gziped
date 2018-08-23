TARGET = gziped.js
LIBS =
CC = emcc
CFLAGS = -I../c/ -O2 -s WASM=1

.PHONY: default all clean

SRC=wasm.c
default: $(TARGET)
all: default
re: clean all

HEADERS = $(wildcard *.h)

$(TARGET):
	$(CC) $(CFLAGS) $(SRC) -o $@

clean:
	-rm -f *.wasm
	-rm -f $(TARGET)
