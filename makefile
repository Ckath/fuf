NAME = fuf
VERSION = 0.1
CFLAGS = 
LIBS = -lncurses -lpthread
SRC = ${NAME}.c ext/colors.c ext/sort.c ext/thr.c ext/sysext.c
OBJ = ${SRC:.c=.o}
DESTDIR = /usr
CC = gcc

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
	@mkdir -p /usr/lib/${NAME}
	@cp -f scripts/* /usr/lib/${NAME}
uninstall: ${NAME}
	@echo removing executable file from ${DESTDIR}/bin
	@rm -f ${DESTDIR}/bin/${NAME}
	@echo removing scripts
	@rm -rf /usr/lib/${NAME}
