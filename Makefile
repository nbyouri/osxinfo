CC ?= clang
PREFIX ?= /usr/pkg

VARBASE ?= /var
PKGIN_DB = ${VARBASE}/db/pkgin/pkgin.db

CFLAGS = -I/usr/include -DPKGIN_DB=\"${PKGIN_DB}\"
LDFLAGS = -L/usr/lib -lsqlite3 -framework IOKit -framework Foundation
all:
	${CC} ${CFLAGS} info.c ${LDFLAGS} -o osxinfo
install:
	test -d ${DESTDIR}${PREFIX}/bin || mkdir -p ${DESTDIR}${PREFIX}/bin
	install -pm 755 osxinfo ${DESTDIR}${PREFIX}/bin
clean:
	rm osxinfo
