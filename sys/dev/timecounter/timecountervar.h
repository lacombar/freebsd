#ifndef DEV_TIMECOUNTER_H
#define DEV_TIMECOUNTER_H

struct timecounter_ivars
{
	const char *		ti_desc;
	struct timecounter *	ti_tc;
};

extern devclass_t	timecounter_devclass;
extern driver_t		timecounter_driver;

#endif /* DEV_TIMECOUNTER_H */

