.POSIX:
VERSION = 1.0
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

CC=cc
CFLAGS = -Os -std=c2x -D_DEFAULT_SOURCE -Wall -Wextra -I/usr/X11R6/include
LDFLAGS = -L/usr/X11R6/lib
LDLIBS = -lX11 -lpng -lwebp

SRC = xscreen.c
BIN = xscreen

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(BIN)

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(BIN) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(BIN)
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < xscreen.1 > $(DESTDIR)$(MANPREFIX)/man1/xscreen.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/xscreen.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
		$(DESTDIR)$(MANPREFIX)/man1/xscreen.1
