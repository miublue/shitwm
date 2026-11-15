CC = tcc
LIBS = `pkg-config --libs --cflags x11 xft`
PREFIX = /usr/local
CFLAGS = -Wall -Werror -Wextra -D_DEFAULT_SOURCE

all:
	${CC} -O2 -o shitwm shitwm.c -I. ${CFLAGS} ${LIBS}

install: all
	mkdir -p ${PREFIX}/bin
	install -s shitwm ${PREFIX}/bin
	cp shitwm.desktop /usr/share/xsessions/

uninstall:
	rm ${PREFIX}/bin/shitwm /usr/share/xsessions/shitwm.desktop

.PHONY: all install uninstall
