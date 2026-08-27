CC = gcc
AR = ar

CFLAGS = -Wall -Wextra -O2 -static

TARGET = libphantom.a

OBJECTS = \
	phantom.o \
	phantom_osd.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

phantom.o: phantom.c phantom.h
	$(CC) $(CFLAGS) -c phantom.c -o phantom.o

phantom_osd.o: phantom_osd.c phantom_osd.h phantom_osd_uapi.h
	$(CC) $(CFLAGS) -c phantom_osd.c -o phantom_osd.o

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
