# swt - simple widget toolkit

include config.mk

SRC = drw.c swt.c util.c
OBJ = ${SRC:.c=.o}

all: options swt

options:
	@echo swt build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

swt: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f swt ${OBJ} swt-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p swt-${VERSION}
	@cp -R LICENSE Makefile README config.mk swt.c swt-${VERSION}
	@tar -cf swt-${VERSION}.tar swt-${VERSION}
	@gzip swt-${VERSION}.tar
	@rm -rf swt-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f swt ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/swt

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/swt

.PHONY: all options clean dist install uninstall
