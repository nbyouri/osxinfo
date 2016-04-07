CC ?= clang
PREFIX ?= /opt/pkg

VARBASE ?= ${PREFIX}/var
DBDIR ?= ${VARBASE}/db/pkg

CFLAGS =  -I/usr/include -DDBDIR=\"${DBDIR}\" -g -Wall
LDFLAGS = -L/usr/lib -framework IOKit -framework Foundation

all:
	${CC} ${CFLAGS} info.c ${LDFLAGS} -o osxinfo
install:
	test -d ${DESTDIR}${PREFIX}/bin || mkdir -p ${DESTDIR}${PREFIX}/bin
	install -pm 755 osxinfo ${DESTDIR}${PREFIX}/bin
clean:
	rm osxinfo
