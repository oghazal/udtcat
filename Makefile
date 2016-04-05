CC=g++
CFLAGS=-c -g -O0 -Iinclude -Wall -Wextra
LDFLAGS=-ludt -lpthread

TARGET = udtcat
PREFIX = $(DESTDIR)/usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1
MANFILE= $(TARGET).1

all: $(TARGET)

$(TARGET): udt-wrapper.o udtcat.o
	$(CC) $(LDFLAGS) udt-wrapper.o udtcat.o -o udtcat
udt-wrapper.o: udt-wrapper.cpp
	$(CC) $(CFLAGS)  udt-wrapper.cpp
udtcat.o: udtcat.c
	$(CC) $(CFLAGS)  udtcat.c

install:
	install -D $(TARGET) $(BINDIR)/$(TARGET)
	install -D $(MANFILE) $(MANDIR)

install-strip:
	install -D -s $(TARGET) $(BINDIR)/$(TARGET)
	install -D $(MANFILE) $(MANDIR) 

uninstall:
	-rm -f $(BINDIR)/$(TARGET) $(MANDIR)/$(MANFILE)

clean:
	-rm -rf *.o $(TARGET)

.PHONY : all clean install install-strip uninstall
