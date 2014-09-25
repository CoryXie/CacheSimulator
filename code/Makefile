CC=gcc

CFLAGS=-c -Wall

LDFLAGS=-lm

SOURCES=main.c cache.c debug.c

OBJECTS=$(SOURCES:.c=.o)

EXECUTABLE=sim

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o $(EXECUTABLE)
