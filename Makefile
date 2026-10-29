CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2
TARGET = cp
SRC = main.c
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)

	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: all clean
