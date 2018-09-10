CFLAGS:=-std=c99 -Wall -Wextra -O2 -shared -fpic
LDFLAGS:=
SOURCES:=circletc.c mongoose.c
HEADERS:=mongoose.h
OBJECTS:=$(patsubst %.c,%.o,${SOURCES})
TARGET:=circletc.so

all: ${TARGET}

%.o: %.c ${HEADERS}
	${CC} ${CFLAGS} -o $@ -c $< ${LDFLAGS}

${TARGET}: ${OBJECTS}
	${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS}

clean: 
	rm ${OBJECTS}
	rm ${TARGET}

install: ${TARGET}
	cp ${TARGET} ${JANET_PATH}
	cp circlet.janet ${JANET_PATH}

.PHONY: all clean install

