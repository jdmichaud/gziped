TARGET = gziped
TEST_TARGET = test
LIBS =
CC = gcc
CFLAGS = -std=c99 -ggdb3 -Wall -Wextra
#CFLAGS = -std=c99 -O3 -Wall -Wextra
LDFALGS = -L./

.PHONY: default all clean

default: $(TARGET)
all: default
re: clean all

OBJECTS = main.o
TEST_OBJECTS = test.o
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFALGS) $(OBJECTS) $(LIBS) -o $@

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) $(LDFALGS) $(TEST_OBJECTS) $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)
	-rm -f $(TEST_TARGET)