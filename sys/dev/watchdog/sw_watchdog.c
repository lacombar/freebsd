/*-
 * Copyright (c) 2011 Arnaud Lacombe
 * All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_kdb.h"
#include "opt_sw_watchdog.h"

#ifndef SW_WATCHDOG
#error "Trying to build sw_watchdog(4) without SW_WATCHDOG"
#endif
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/kdb.h>
#include <sys/module.h>
#include <sys/time.h>

#include <dev/watchdog/watchdogvar.h>

#include "watchdog_if.h"

extern void (*sw_watchdog_poke_p)(int);

struct sw_watchdog_softc
{
	int	sws_enabled;
	int	sws_ticks;

	/* configuration */
	int	sws_timeout;
	watchdog_action_t	sws_action;
};

/*
 * Prototypes declarations
 */
/* Device interface */
static void sw_watchdog_identify(driver_t *, device_t);
static int sw_watchdog_probe(device_t);
static int sw_watchdog_attach(device_t);

/* Watchdog interface */
static int sw_watchdog_enable(device_t);
static int sw_watchdog_disable(device_t);
static int sw_watchdog_configure(device_t, struct timespec *,
	    watchdog_action_t);
static int sw_watchdog_rearm(device_t);

/* Miscellaneous */
static void sw_watchdog_poke(int);
static void sw_watchdog_fire(void);

/*
 *
 */
static device_t sw_watchdog_dev;

static struct watchdog_ivars sw_watchdog_ivars =
{
	.wi_step    = WATCHDOG_TIMESPEC_STEP(0, 0),
	.wi_nstep   = 0,
	.wi_actions = WATCHDOG_ACTION_PANIC
};

static device_method_t sw_watchdog_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_identify,	sw_watchdog_identify),
	DEVMETHOD(device_probe,		sw_watchdog_probe),
	DEVMETHOD(device_attach,	sw_watchdog_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Watchdog interface */
	DEVMETHOD(watchdog_enable,	sw_watchdog_enable),
	DEVMETHOD(watchdog_disable,	sw_watchdog_disable),
	DEVMETHOD(watchdog_configure,	sw_watchdog_configure),
	DEVMETHOD(watchdog_rearm,	sw_watchdog_rearm),

	/**/
	DEVMETHOD_END
};

static driver_t sw_watchdog_driver =
{
	"sw_watchdog",
	sw_watchdog_methods,
	sizeof (struct sw_watchdog_softc)
};

static devclass_t sw_watchdog_devclass;

DRIVER_MODULE(sw_watchdog, nexus, sw_watchdog_driver, sw_watchdog_devclass, 0, 0);
DRIVER_MODULE(watchdog, sw_watchdog, watchdog_driver, watchdog_devclass, 0, 0);

/*
 * Function definitions
 */

/* Device interface */
static void
sw_watchdog_identify(driver_t *driver, device_t parent)
{

	if (BUS_ADD_CHILD(parent, 0, "sw_watchdog", 0) == NULL)
		panic(__func__);
}

static int
sw_watchdog_probe(device_t dev)
{

	device_set_desc(dev, "Software watchdog");
	return 0;
}

static int
sw_watchdog_attach(device_t dev)
{
	struct bintime bt = WATCHDOG_TIMEVAL_STEP(0, tick);
	device_t child;

	sw_watchdog_dev = dev;

	sw_watchdog_ivars.wi_step = bt;
	sw_watchdog_ivars.wi_nstep = 86400 * tick;
#if defined(KDB)
	sw_watchdog_ivars.wi_nstep |= WATCHDOG_ACTION_DEBUGGER;
#endif

	child = device_add_child(dev, "watchdog", -1);
	if (child == NULL) {
		device_printf(dev, "failed to add watchdog");
	} else {
		device_set_ivars(child, &sw_watchdog_ivars);
		sw_watchdog_poke_p = sw_watchdog_poke;
	}

	return bus_generic_attach(dev);
}

/* Watchdog interface */
static int
sw_watchdog_enable(device_t dev)
{
	struct sw_watchdog_softc *sws = device_get_softc(dev);

	sws->sws_enabled = 1;
	
	return 0;
}

static int
sw_watchdog_disable(device_t dev)
{
	struct sw_watchdog_softc *sws = device_get_softc(dev);

	sws->sws_enabled = 0;

	return 0;
}

static int
sw_watchdog_configure(device_t dev, struct timespec *ts,
    watchdog_action_t action)
{
	struct sw_watchdog_softc *sws = device_get_softc(dev);
	int error;

	sws->sws_timeout = ts->tv_sec * hz;

	switch (sws->sws_action) {
#if defined(KDB)
	case WATCHDOG_ACTION_DEBUGGER:
#endif
	case WATCHDOG_ACTION_PANIC:
		sws->sws_action = action;
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

static int
sw_watchdog_rearm(device_t dev)
{
	struct sw_watchdog_softc *sws = device_get_softc(dev);

	sws->sws_ticks = 0;

	return 0;
}

/* Miscellaneous */
static void
sw_watchdog_poke(int ticks)
{
	struct sw_watchdog_softc *sws = device_get_softc(sw_watchdog_dev);

	if (sws->sws_enabled <= 0)
		return;

	atomic_fetchadd_int(&sws->sws_ticks, ticks);
	if (sws->sws_ticks >= sws->sws_timeout)
		sw_watchdog_fire();
}

static void
sw_watchdog_fire()
{
	struct sw_watchdog_softc *sws = device_get_softc(sw_watchdog_dev);

	switch (sws->sws_action) {
#if defined(KDB)
	case WATCHDOG_ACTION_DEBUGGER:
		kdb_backtrace();
		kdb_enter(KDB_WHY_WATCHDOG, "watchdog timeout");
		sw_watchdog_rearm(sw_watchdog_dev);
		break;
#endif
	case WATCHDOG_ACTION_PANIC:
		panic("watchdog timeout");
		break;
	}
}
