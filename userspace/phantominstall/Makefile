CC = gcc

CFLAGS = -Wall -Wextra -O2 -static -I. -I../libphantom

TARGET = phatominstall

LIBDIR = ../libphantom
LIB = $(LIBDIR)/libphantom.a

SOURCES = \
	main.c \
	installer.c \
	system.c \
	network.c \
	storage.c \
	package.c

OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(LIB):
	$(MAKE) -C $(LIBDIR)

$(TARGET): $(OBJECTS) $(LIB)
	$(CC) $(CFLAGS) $(OBJECTS) $(LIB) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
	$(MAKE) -C $(LIBDIR) clean

.PHONY: all clean
