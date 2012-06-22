#ifndef DEV_WATCHDOG_H
#define DEV_WATCHDOG_H

#define	WATCHDOG_ACTION_IRQ		(1<<0)
#define	WATCHDOG_ACTION_NMI		(1<<1)
#define	WATCHDOG_ACTION_RESET		(1<<2)
#define	WATCHDOG_ACTION_DEBUGGER	(1<<30)
#define	WATCHDOG_ACTION_PANIC		(1<<31)

#define WATCHDOG_ACTION_DESC						\
	"\20"								\
	"\1irq"								\
	"\2nmi"								\
	"\3reset"							\
	"\37debugger"							\
	"\40panic"

typedef uint32_t watchdog_action_t;

#define WATCHDOG_TIMESPEC_STEP(s, nsec) 				\
	{								\
		.sec = (s),						\
		.frac = (nsec) * (uint64_t)18446744073LL /* see <sys/time.h> */ \
	}

#define WATCHDOG_TIMEVAL_STEP(s, usec) 					\
	{								\
		.sec = (s),						\
		.frac = (usec) * (uint64_t)18446744073709LL /* see <sys/time.h> */\
	}

struct watchdog_ivars
{
	struct bintime		wi_step;
	unsigned int		wi_nstep;
	watchdog_action_t	wi_actions;
};

static __inline void
watchdog_get_min_timeout(device_t dev, struct timespec *ts)
{
	struct watchdog_ivars *wi = device_get_ivars(dev);

	bintime2timespec(&wi->wi_step, ts);
}

static __inline void
watchdog_get_max_timeout(device_t dev, struct timespec *ts)
{
	struct watchdog_ivars *wi = device_get_ivars(dev);
	struct bintime bt;

	bt.sec = wi->wi_step.sec;
	bt.frac = wi->wi_step.frac;

	bintime_mul(&bt, wi->wi_nstep);
	bintime2timespec(&bt, ts);
}

extern devclass_t	watchdog_devclass;
extern driver_t		watchdog_driver;

#endif /* DEV_WATCHDOG_H */
