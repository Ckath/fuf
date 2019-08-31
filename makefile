NAME = fuf
VERSION = 0.1
CFLAGS = 
LIBS = -lncurses -lpthread
SRC = ${NAME}.c ext/colors.c ext/sort.c ext/thr.c ext/sysext.c
OBJ = ${SRC:.c=.o}
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

strap: ${NAME}
	@echo setting up ${NAME} 
	@mkdir -p ~/.config/fuf
	@sudo ln -svf "$(CURDIR)/fuf" /bin
	@ln -svf "$(CURDIR)/scripts/open" ~/.config/fuf
	@ln -svf "$(CURDIR)/scripts/preview" ~/.config/fuf
