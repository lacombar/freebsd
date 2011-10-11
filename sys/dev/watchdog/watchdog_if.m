# $FreeBSD$

#include <sys/bus.h>
#include <sys/watchdog.h>

#include <dev/watchdog/watchdogvar.h>

INTERFACE watchdog;

#
#
#
METHOD int enable {
	device_t		dev;
};

#
#
#
METHOD int disable {
	device_t		dev;
};

#
#
#
METHOD int configure {
	device_t		dev;
	struct timespec *	ts;
	watchdog_action_t	action;
};

#
#
#
METHOD int rearm {
	device_t		dev;
};
