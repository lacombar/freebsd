# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/watchdog

KMOD=	watchdog
SRCS=	watchdog.c
SRCS+=	bus_if.h device_if.h
SRCS+=	watchdog_if.c watchdog_if.h

EXPORT_SYMS=	watchdog_devclass \
		watchdog_driver

.include <bsd.kmod.mk>
