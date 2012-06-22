# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/watchdog

KMOD=	watchdog
SRCS=	watchdog.c
SRCS+=	watchdog_if.c
SRCS+=	bus_if.h
SRCS+=	device_if.h
SRCS+=	watchdog_if.h

EXPORT_SYMS=	watchdog_devclass
EXPORT_SYMS+=	watchdog_driver

.include <bsd.kmod.mk>
