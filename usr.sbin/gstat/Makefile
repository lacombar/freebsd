# $FreeBSD$

PROG=	gstat
MAN=	gstat.8
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} ${LIBEDIT} ${LIBCURSES}
LDADD=	-ldevstat -lkvm -lgeom -lbsdxml -lsbuf -ledit -lcurses

.include <bsd.prog.mk>
