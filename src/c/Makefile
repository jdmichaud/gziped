TARGET = gziped
TEST_TARGET = test
LIBS =
CC = gcc
CFLAGS = -std=c99 -g -Wall

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = main.c
TEST_OBJECTS = test.c
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(CFLAGS) $(LIBS) -o $@

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) $(TEST_OBJECTS) $(CFLAGS) $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)
	-rm -f $(TEST_TARGET)