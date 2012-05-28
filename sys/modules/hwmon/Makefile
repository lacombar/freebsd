# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/hwmon
KMOD=	hwmon
SRCS=	hwmon.c
SRCS+=	hwmon_if.c
SRCS+=	bus_if.h
SRCS+=	device_if.h
SRCS+=	hwmon_if.h

GXPORT_SYMS=	hwmon_devclass
EXPORT_SYMS+=	hwmon_driver
EXPORT_SYMS+=	hwmon_get_name_desc
EXPORT_SYMS+=	hwmon_get_update_interval_desc
EXPORT_SYMS+=	hwmon_in_get_average_desc
EXPORT_SYMS+=	hwmon_in_get_crit_desc
EXPORT_SYMS+=	hwmon_in_get_highest_desc
EXPORT_SYMS+=	hwmon_in_get_input_desc
EXPORT_SYMS+=	hwmon_in_get_label_desc
EXPORT_SYMS+=	hwmon_in_get_lcrit_desc
EXPORT_SYMS+=	hwmon_in_get_lowest_desc
EXPORT_SYMS+=	hwmon_in_get_max_desc
EXPORT_SYMS+=	hwmon_in_get_min_desc
EXPORT_SYMS+=	hwmon_in_reset_history_desc
EXPORT_SYMS+=	hwmon_in_set_crit_desc
EXPORT_SYMS+=	hwmon_in_set_lcrit_desc
EXPORT_SYMS+=	hwmon_in_set_max_desc
EXPORT_SYMS+=	hwmon_in_set_min_desc

.include <bsd.kmod.mk>
