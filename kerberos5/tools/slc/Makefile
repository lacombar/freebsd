# $FreeBSD$

PROG=	slc
LIBROKEN_A=     ${.OBJDIR}/../../lib/libroken/libroken.a
LDADD=  ${LIBROKEN_A} ${LIBVERS}
DPADD=  ${LIBROKEN_A} ${LIBVERS}
NO_MAN=

SRCS=	roken.h \
	slc-gram.y \
	slc-lex.l

CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/sl -I${KRB5DIR}/lib/vers -I.

CLEANFILES= roken.h

roken.h:
	${MAKE_ROKEN} > ${.TARGET}

#	${.OBJDIR}/../make-roken/make-roken > ${.TARGET}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/roken ${KRB5DIR}/lib/sl
