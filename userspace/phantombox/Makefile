CC = gcc

CFLAGS = -Wall -Wextra -O2 -static

TARGET = phantombox

LIBDIR = ../libphantom
LIB = $(LIBDIR)/libphantom.a

all: $(TARGET)

$(LIB):
	$(MAKE) -C $(LIBDIR)

$(TARGET): main.o $(LIB)
	$(CC) $(CFLAGS) main.o $(LIB) -o $(TARGET)

main.o: main.c
	$(CC) $(CFLAGS) -I$(LIBDIR) -c main.c -o main.o

clean:
	rm -f main.o $(TARGET)

.PHONY: all clean
