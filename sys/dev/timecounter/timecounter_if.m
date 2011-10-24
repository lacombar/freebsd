# $FreeBSD$

#include <sys/bus.h>

#include <dev/timecounter/timecountervar.h>

INTERFACE timecounter;

#
#
#
METHOD int init {
	device_t		dev;
};

#
#
#
METHOD u_int xget {
	device_t		dev;
};

