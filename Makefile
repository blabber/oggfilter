PROG=		oggfilter
SRCS=		${PROG}.c options.c checks.c list.c

CSTD?=		c99
WARNS?=		6
WFORMAT?=	1
# NO_WERROR is needed as libvorbis.h defines some static variables not used
# in this code. This leads to warnings and breaks the build if NO_WERROR is
# unset.  But I prefer NO_WERROR to hiding the warning.
NO_WERROR=	yes

CFLAGS+=	-D_POSIX_C_SOURCE=200809 \
		-I/usr/local/include
LDFLAGS+=	-L/usr/local/lib 
LDADD+=		-lvorbisfile ${ICONV_LIB}

CLEANFILES=	*.[Bb][Aa][Kk] *.core
CTAGS=		ctags

COL=		/usr/bin/col

all:		README

.include <bsd.prog.mk>

README:	${MAN1}
	${MROFF_CMD} ${MAN1} | ${COL} -bx > ${.TARGET}
