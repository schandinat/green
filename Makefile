CFLAGS	:=	-Os -Wall

CC	:=	cc
RM	:=	rm -f
INSTALL	:=	install

DESTDIR	:=	
PREFIX	:=	/usr/local
BINDIR	:=	$(PREFIX)/bin

POPPLER_CFLAGS	:=	$$(pkg-config poppler-glib --cflags)
POPPLER_LIBS	:=	$$(pkg-config poppler-glib --libs)
SDL_CFLAGS	:=	$$(sdl-config --cflags)
SDL_LIBS	:=	$$(sdl-config --libs)


all: green

clean:
	$(RM) green main.o green.o sdl.o

install: green
	$(INSTALL) green $(DESTDIR)/$(BINDIR)/


green: main.o green.o sdl.o
	$(CC) $^ $(POPPLER_LIBS) $(SDL_LIBS) -o $@

main.o: main.c green.h
	$(CC) $(CFLAGS) -c $< $(POPPLER_CFLAGS) -o $@

green.o: green.c green.h
	$(CC) $(CFLAGS) -c $< $(POPPLER_CFLAGS) -o $@

sdl.o: sdl.c green.h
	$(CC) $(CFLAGS) -c $< $(POPPLER_CFLAGS) $(SDL_CFLAGS) -o $@
