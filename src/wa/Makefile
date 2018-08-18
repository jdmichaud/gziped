TARGET = gziped.js
LIBS =
CC = emcc
CFLAGS = -I../c/ -s WASM=1 -Os

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
