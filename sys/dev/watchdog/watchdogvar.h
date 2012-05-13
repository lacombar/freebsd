#ifndef DEV_WATCHDOG_H
#define DEV_WATCHDOG_H

#define	WATCHDOG_ACTION_IRQ1		0x00001
#define	WATCHDOG_ACTION_IRQ2		0x00002
#define	WATCHDOG_ACTION_IRQ3		0x00004
#define	WATCHDOG_ACTION_IRQ4		0x00008
#define	WATCHDOG_ACTION_IRQ5		0x00010
#define	WATCHDOG_ACTION_IRQ6		0x00020
#define	WATCHDOG_ACTION_IRQ7		0x00040
#define	WATCHDOG_ACTION_IRQ8		0x00080
#define	WATCHDOG_ACTION_IRQ9		0x00100
#define	WATCHDOG_ACTION_IRQ10		0x00200
#define	WATCHDOG_ACTION_IRQ11		0x00400
#define	WATCHDOG_ACTION_IRQ12		0x00800
#define	WATCHDOG_ACTION_IRQ13		0x01000
#define	WATCHDOG_ACTION_IRQ14		0x02000
#define	WATCHDOG_ACTION_IRQ15		0x04000
#define	WATCHDOG_ACTION_NMI		0x08000
#define	WATCHDOG_ACTION_RESET		0x10000
#define	WATCHDOG_ACTION_DEBUGGER	0x40000000
#define	WATCHDOG_ACTION_PANIC		0x80000000

#define WATCHDOG_ACTION_DESC						\
	"\20"								\
	"\1irq1\2irq2\3irp3\4irq4"					\
	"\5irq5\6irq6\7irq7\10irq8"					\
	"\11irq9\12irq10\13irq11\14irq12"				\
	"\15irq13\16irq14\17irq15\20nmi"				\
	"\21reset"							\
	"\37debugger\40panic"

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
	const char *		wi_desc;
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
