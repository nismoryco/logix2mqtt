# Makefile for logix2mqtt

INCLUDES=

CFLAGS=-c -Wall -DNDEBUG

LDFLAGS=-L. -lplctag -lmosquitto -lcjson

SOURCES=main.c util.c config.c

OBJECTS=$(SOURCES:.c=.o)

EXECUTABLE=logix2mqtt

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

clean:
	find . -name '*.o' -print -delete
	rm -rf $(EXECUTABLE)
