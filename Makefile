PROG=		oggfilter
SRCS=		${PROG}.c options.c checks.c list.c

CSTD?=		c89
WARNS?=		6
WFORMAT?=	1
# NO_WERROR is needed as libvorbis.h defines some static variables not used
# in this code. This leads to warnings and breaks the build if NO_WERROR is
# unset.  But I prefer NO_WERROR to hiding the warning.
NO_WERROR=	yes

CFLAGS+=	-I/usr/local/include
LDFLAGS+=	-L/usr/local/lib 
LDADD+=		-lvorbisfile -liconv

CLEANFILES=	*.[Bb][Aa][Kk] *.core
CTAGS=		ctags

COL=		/usr/bin/col
FIND=		/usr/bin/find
INDENT=		/usr/bin/indent
M4=		/usr/bin/m4
XARGS=		/usr/bin/xargs

all:		README indent

.include <bsd.prog.mk>

indent: .PHONY
	${FIND} . -type f -name '*.[c,h]' | ${XARGS} -n 1 ${INDENT}

README:	${MAN1}
	${MROFF_CMD} ${MAN1} | ${COL} -bx > ${.TARGET}
