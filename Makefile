PROG=           oggfilter

CLEANFILES=     *.[Bb][Aa][Kk] *.core

CFLAGS+=        -Wall --ansi --pedantic
CFLAGS+=        -I/usr/local/include
LDFLAGS+=       -L/usr/local/lib 
LDADD+=         -lvorbisfile -liconv

CTAGS=          ctags

.include <bsd.prog.mk>
