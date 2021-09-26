NAME = fuf
VERSION = "$(shell git describe --tag)"
CFLAGS = -Os
LIBS = -lncurses -lpthread
SRC = ${NAME}.c inc/colors.c inc/sort.c inc/thr.c inc/sysext.c inc/sncurses.c
OBJ = ${SRC:.c=.o}
DESTDIR = /usr
CC = gcc

XHACKS := yes
ifeq ($(XHACKS),yes)
LIBS += -DX_HACKS -lX11
SRC += inc/xhacks.c
OBJ = ${SRC:.c=.o}
endif

.c.o:
	@echo CC -c ${CFLAGS} $<
	@${CC} -c ${CFLAGS} -DVERSION=\"${VERSION}\" $< ${LIBS} -o ${<:.c=.o}

${NAME}: ${SRC} ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${CFLAGS} ${OBJ} ${LIBS}

clean:
	@echo cleaning...
	@rm -f ${NAME} ${OBJ}

install: ${NAME}
	@echo installing executable file to ${DESTDIR}/bin
	@mkdir -p ${DESTDIR}/bin
	@cp -f ${NAME} ${DESTDIR}/bin/${NAME}
	@chmod 755 ${DESTDIR}/bin/${NAME}
	@echo installing scripts
	@mkdir -p ${DESTDIR}/lib/${NAME}
	@cp -f scripts/* ${DESTDIR}/lib/${NAME}
	@cp -f completions/zsh/_${NAME} ${DESTDIR}/share/zsh/site-functions/
	@cp -f completions/bash/${NAME} ${DESTDIR}/share/bash-completion/completions/
uninstall: ${NAME}
	@echo removing executable file from ${DESTDIR}/bin
	@rm -f ${DESTDIR}/bin/${NAME}
	@echo removing scripts
	@rm -rf ${DESTDIR}/lib/${NAME}
	@rm ${DESTDIR}/share/zsh/site-functions/_${NAME}
	@rm ${DESTDIR}/share/bash-completion/completions/${NAME}
