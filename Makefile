PROG=           oggfilter

NO_MAN=         yes

CLEANFILES=     *.BAK *.core *.bak

CFLAGS+=        -Wall --ansi --pedantic
CFLAGS+=        -I/usr/local/include
LDFLAGS+=       -L/usr/local/lib 
LDADD+=	        -lvorbisfile

CTAGS=          ctags

.include <bsd.prog.mk>
