CFLAGS	:=	-Os -Wall

CC	:=	cc
RM	:=	rm -f
INSTALL	:=	install

DESTDIR		:=	
PREFIX		:=	/usr/local
BINDIR		:=	$(PREFIX)/bin
SYSCONFDIR	:=	$(PREFIX)/etc
MANDIR		:=	/usr/share/man

CONFIG	:=	-D GREEN_SYSCONFIG_FILE=\"$(SYSCONFDIR)/green.conf\" -D GREEN_USERCONFIG_FILE=\".green.conf\"

POPPLER_CFLAGS	:=	$$(pkg-config poppler-glib --cflags)
POPPLER_LIBS	:=	$$(pkg-config poppler-glib --libs)
SDL_CFLAGS	:=	$$(sdl-config --cflags)
SDL_LIBS	:=	$$(sdl-config --libs)

all: green

clean:
	$(RM) green main.o green.o sdl.o

localinstall: green
	$(INSTALL) green ~/.local/bin

install: green
	$(INSTALL) green $(DESTDIR)/$(BINDIR)/
	$(INSTALL) green.1 $(MANDIR)/man1/

green: main.o green.o sdl.o
	$(CC) $^ $(POPPLER_LIBS) $(SDL_LIBS) -o $@

main.o: main.c green.h
	$(CC) $(CONFIG) $(CFLAGS) -c $< $(POPPLER_CFLAGS) -o $@

green.o: green.c green.h
	$(CC) $(CFLAGS) -c $< $(POPPLER_CFLAGS) -o $@

sdl.o: sdl.c green.h
	$(CC) $(CFLAGS) -c $< $(POPPLER_CFLAGS) $(SDL_CFLAGS) -o $@
