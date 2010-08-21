# New ports collection makefile for:	oggfilter
# Date created:			06 Nov 2009
# Whom:				Tobias Rehbein <tobias.rehbein@web.de>
#
# $FreeBSD$

PORTNAME=	oggfilter
PORTVERSION=	1.3.2
CATEGORIES=	audio
MASTER_SITES=	# No master site
DISTFILES=	${PORTNAME}-${TAG}.tar

MAINTAINER=	tobias.rehbein@web.de
COMMENT=	A simple command-line tool used to filter a list of ogg/vorbis files

FETCH_DEPENDS=	git:${PORTSDIR}/devel/git
LIB_DEPENDS=	vorbis.4:${PORTSDIR}/audio/libvorbis

USE_ICONV=	yes

MAKE_ARGS=	BINDIR=${PREFIX}/bin \
		MANDIR=${PREFIX}/man/man \
		NO_MANCOMPRESS=yes

#XXX Let this point to your local clone of the git repository
REPO?=		/path/to/repo/clone/

TAG=		v${PORTVERSION}
WRKSRC=		${WRKDIR}/${PORTNAME}-${TAG}

PLIST_FILES=	bin/oggfilter
MAN1=		oggfilter.1

do-fetch:
	( cd "${REPO}" \
		&& ${LOCALBASE}/bin/git archive \
			--prefix "${PORTNAME}-${TAG}/" \
			-o ${DISTDIR}/${DISTFILES} \
			${TAG} )

.include <bsd.port.mk>
