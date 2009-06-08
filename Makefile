PROG=		oggfilter
SRCS=		${PROG}.c options.c

CLEANFILES=	*.[Bb][Aa][Kk] *.core ${MAN1}.txt

CFLAGS+=	-Wall --ansi --pedantic
CFLAGS+=	-I/usr/local/include
LDFLAGS+=	-L/usr/local/lib 
LDADD+=		-lvorbisfile -liconv

CTAGS=		ctags

README=		README.markdown

COL=		/usr/bin/col
FIND=		/usr/bin/find
INDENT=		/usr/bin/indent
M4=		/usr/bin/m4
SED=		/usr/bin/sed
XARGS=		/usr/bin/xargs

all:		${README}

.include <bsd.prog.mk>

indent: .PHONY
	${FIND} . -type f -name '*.[c,h]' | ${XARGS} -n 1 ${INDENT}

${MAN1}.txt:	${MAN1}
	${MROFF_CMD} ${MAN1} | ${COL} -bx | ${SED} 's/^/    /' > ${.TARGET}

${README}:	${README}.m4 ${MAN1}.txt
	${M4} ${README}.m4 > ${README} 
