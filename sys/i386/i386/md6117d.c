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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/watchdog.h>
#include <sys/bus.h>
#include <machine/bus.h>

#include <dev/watchdog/watchdogvar.h>

#include <i386/i386/md6117d.h>

#include "watchdog_if.h"

static int	md6117d_probe(device_t);
static int	md6117d_attach(device_t);

static __inline void	md6117d_lock_register(void);
static __inline void	md6117d_unlock_register(void);
static __inline void	md6117d_configure(unsigned int, unsigned int);
static __inline void	md6117d_enable(void);
static __inline void	md6117d_disable(void);
static __inline void	md6117d_reset(void);

/* Watchdog interface */
static int	md6117d_watchdog_enable(device_t);
static int	md6117d_watchdog_disable(device_t);
static int	md6117d_watchdog_configure(device_t, struct timespec *,
		    watchdog_action_t);
static int	md6117d_watchdog_rearm(device_t);

/*
 * Interfaces & driver glue
 */
static device_method_t md6117d_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_probe,		md6117d_probe),
	DEVMETHOD(device_attach,	md6117d_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),

	/* Watchdog interface */
	DEVMETHOD(watchdog_enable,	md6117d_watchdog_enable),
	DEVMETHOD(watchdog_disable,	md6117d_watchdog_disable),
	DEVMETHOD(watchdog_configure,	md6117d_watchdog_configure),
	DEVMETHOD(watchdog_rearm,	md6117d_watchdog_rearm),
	DEVMETHOD_END
};

static driver_t md6117d_driver =
{
	.name    = "md6117d",
	.methods = md6117d_methods,
};

static devclass_t md6117d_devclass;

DRIVER_MODULE(md6117d, vortex, md6117d_driver, md6117d_devclass, NULL, NULL);
DRIVER_MODULE(watchdog, md6117d, watchdog_driver, watchdog_devclass, NULL, NULL);

static struct watchdog_ivars md6117d_watchdog_ivars =
{
	.wi_step    = WATCHDOG_TIMEVAL_STEP(0, WDT0_MIN_TIMEOUT),
	.wi_nstep   = 1<<WDT0_COUNTER_WIDTH,
	.wi_actions = WATCHDOG_ACTION_IRQ3 | WATCHDOG_ACTION_IRQ4 |
	              WATCHDOG_ACTION_IRQ5 | WATCHDOG_ACTION_IRQ6 |
	              WATCHDOG_ACTION_IRQ7 | WATCHDOG_ACTION_IRQ9 |
	              WATCHDOG_ACTION_IRQ10 | WATCHDOG_ACTION_IRQ12 |
	              WATCHDOG_ACTION_IRQ14 | WATCHDOG_ACTION_IRQ15 |
	              WATCHDOG_ACTION_NMI | WATCHDOG_ACTION_RESET
};

/*
 * Device interface
 */
static int
md6117d_probe(device_t dev)
{

	device_set_desc(dev, "DMP md6117d-compatible watchdog");
	return 0;
}

static int
md6117d_attach(device_t dev)
{
	device_t child;

	child = device_add_child(dev, "md6117d", -1);
	if (child == NULL)
		device_printf(dev, "unable to add watchdog(4)");
	else
		device_set_ivars(child, &md6117d_watchdog_ivars);

	return bus_generic_attach(dev);
}

/*
 *
 */
static uint8_t
md6117d_readbyte(uint16_t port)
{
	return inb(port);
}

static void
md6117d_writebyte(uint16_t port, uint8_t val)
{
	return outb(port, val);
}

/*
 *
 */

static __inline void
md6117d_lock_register()
{
	md6117d_writebyte(WDT0_PORT_IDX, 0x13);
	md6117d_writebyte(WDT0_PORT_DATA, 0x00);
}

static __inline void
md6117d_unlock_register()
{
	md6117d_writebyte(WDT0_PORT_IDX, 0x13);
	md6117d_writebyte(WDT0_PORT_DATA, 0xc5);
}

static __inline void
md6117d_configure(unsigned int action, unsigned int count)
{
	uint8_t c;

	/* configure action */
	md6117d_writebyte(WDT0_PORT_IDX, WDT0_IDX_ACTION);
	c = md6117d_readbyte(WDT0_PORT_DATA);
	c &= 0x0f;
	c |= action;
	md6117d_writebyte(WDT0_PORT_IDX, WDT0_IDX_ACTION);
	md6117d_writebyte(WDT0_PORT_DATA, c);

	/* configure counter */
	md6117d_writebyte(WDT0_PORT_IDX, WDT_IDX_COUNTER_B2);
	md6117d_writebyte(WDT0_PORT_DATA, (count >> 16) & 0xff);
	md6117d_writebyte(WDT0_PORT_IDX, WDT_IDX_COUNTER_B1);
	md6117d_writebyte(WDT0_PORT_DATA, (count >> 8) & 0xff);
	md6117d_writebyte(WDT0_PORT_IDX, WDT_IDX_COUNTER_B0);
	md6117d_writebyte(WDT0_PORT_DATA, count & 0xff);
}

static __inline void
md6117d_enable()
{
	uint8_t c;

	md6117d_writebyte(WDT0_PORT_IDX, WDT0_IDX_CONTROL);
	c = md6117d_readbyte(WDT0_PORT_DATA);
	c |= WDT_ENABLE;
	md6117d_writebyte(WDT0_PORT_IDX, WDT0_IDX_CONTROL);
	md6117d_writebyte(WDT0_PORT_DATA, c);
}

static __inline void
md6117d_disable()
{
	uint8_t c;

	md6117d_writebyte(WDT0_PORT_IDX, WDT0_IDX_CONTROL);
	c = md6117d_readbyte(WDT0_PORT_DATA);
	c &= ~WDT_ENABLE;
	md6117d_writebyte(WDT0_PORT_IDX, WDT0_IDX_CONTROL);
	md6117d_writebyte(WDT0_PORT_DATA, c);
}

static __inline void
md6117d_reset()
{
	uint8_t c;

	md6117d_writebyte(WDT0_PORT_IDX, WDT0_IDX_STATUS);
	c = md6117d_readbyte(WDT0_PORT_DATA);
	c |= WDT_RESET;
	md6117d_writebyte(WDT0_PORT_IDX, WDT0_IDX_STATUS);
	md6117d_writebyte(WDT0_PORT_DATA, c);
}

/*
 * Watchdog interface
 */
static int
md6117d_watchdog_enable(device_t dev)
{

	device_printf(dev, "- %s -\n", __func__);

	md6117d_unlock_register();
	md6117d_enable();
	md6117d_lock_register();

	return 0;
}

static int
md6117d_watchdog_disable(device_t dev)
{

	device_printf(dev, "- %s -\n", __func__);

	md6117d_unlock_register();
	md6117d_disable();
	md6117d_lock_register();

	return 0;
}

static int
md6117d_watchdog_configure(device_t dev, struct timespec *ts,
    watchdog_action_t action)
{
	unsigned int wdt_action, count;
	int error;

	device_printf(dev, "- %s -\n", __func__);

	count = WDT0_1SEC * ts->tv_sec;

	/* XXX al:
	 * A lookup table might be more flexible to use ...
	 */
	switch (action) {	
	case WATCHDOG_ACTION_IRQ3:
		wdt_action = WDT_ACTION_IRQ3;
		break;
	case WATCHDOG_ACTION_IRQ4:
		wdt_action = WDT_ACTION_IRQ4;
		break;
	case WATCHDOG_ACTION_IRQ5:
		wdt_action = WDT_ACTION_IRQ5;
		break;
	case WATCHDOG_ACTION_IRQ6:
		wdt_action = WDT_ACTION_IRQ6;
		break;
	case WATCHDOG_ACTION_IRQ7:
		wdt_action = WDT_ACTION_IRQ7;
		break;
	case WATCHDOG_ACTION_IRQ9:
		wdt_action = WDT_ACTION_IRQ9;
		break;
	case WATCHDOG_ACTION_IRQ10:
		wdt_action = WDT_ACTION_IRQ10;
		break;
	case WATCHDOG_ACTION_IRQ12:
		wdt_action = WDT_ACTION_IRQ12;
		break;
	case WATCHDOG_ACTION_IRQ14:
		wdt_action = WDT_ACTION_IRQ14;
		break;
	case WATCHDOG_ACTION_IRQ15:
		wdt_action = WDT_ACTION_IRQ15;
		break;
	case WATCHDOG_ACTION_NMI:
		wdt_action = WDT_ACTION_NMI;
		break;
	case WATCHDOG_ACTION_RESET:
		wdt_action = WDT_ACTION_RESET;
		break;
	default:
		error = EINVAL;
		goto out;
	}

	md6117d_unlock_register();
	md6117d_configure(wdt_action, count);
	md6117d_lock_register();

out:
	return error;
}

static int
md6117d_watchdog_rearm(device_t dev)
{

	device_printf(dev, "- %s -\n", __func__);

	md6117d_unlock_register();
	md6117d_reset();
	md6117d_lock_register();

	return 0;
}
