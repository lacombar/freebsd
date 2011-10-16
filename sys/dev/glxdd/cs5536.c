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
#include <sys/timetc.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/watchdog/watchdogvar.h>

#include <i386/i386/cs5536.h>

#ifdef GPIO
#include "gpio_if.h"
#endif

#ifdef SMBUS
#include "smbus_if.h"
#endif

#ifdef WATCHDOG
#include "watchdog_if.h"
#endif

struct cs5536_softc
{
	/* watchdog */
	uint16_t	cs_lbar_mfgpt;
	struct timespec	cs_current_timeout;
};

/*
 * Prototypes declarations
 */
/* Device interface */
static int cs5536_probe(device_t);
static int cs5536_attach(device_t);

#ifdef GPIO
/* GPIO interface */
static int cs5536_gpio_pin_max(device_t, int *);
static int cs5536_gpio_pin_set(device_t, uint32_t, uint32_t);
static int cs5536_gpio_pin_get(device_t, uint32_t, uint32_t *);
static int cs5536_gpio_pin_toggle(device_t, uint32_t);
static int cs5536_gpio_pin_getcaps(device_t, uint32_t, uint32_t *);
static int cs5536_gpio_pin_getflags(device_t, uint32_t, uint32_t *);
static int cs5536_gpio_pin_getname(device_t, uint32_t, char *);
static int cs5536_gpio_pin_setflags(device_t, uint32_t, uint32_t);
#endif

#ifdef SMBUS
/* SMB interface */
static smbus_intr_t cs5536_smbus_intr;
static smbus_callback_t cs5536_smbus_callback;
static smbus_quick_t cs5536_smbus_quick;
static smbus_sendb_t cs5536_smbus_sendb;
static smbus_recvb_t cs5536_smbus_recvb;
static smbus_writeb_t cs5536_smbus_writeb;
static smbus_writew_t cs5536_smbus_writew;
static smbus_readb_t cs5536_smbus_readb;
static smbus_readw_t cs5536_smbus_readw;
static smbus_pcall_t cs5536_smbus_pcall;
static smbus_bwrite_t cs5536_smbus_bwrite;
static smbus_bread_t cs5536_smbus_bread;
#endif

#ifdef WATCHDOG
/* Watchdog interface */
static int cs5536_watchdog_enable(device_t);
static int cs5536_watchdog_disable(device_t);
static int cs5536_watchdog_configure(device_t, struct timespec *,
	    watchdog_action_t);
static int cs5536_watchdog_rearm(device_t);
#endif

/*
 * Children ivars
 */
#ifdef WATCHDOG
static struct watchdog_ivars cs5536_watchdog_ivars =
{
	.wi_step    = WATCHDOG_TIMESPEC_STEP(0, (1000000000 / 32000) * 16384),
	.wi_nstep   = 32000,
	.wi_actions = WATCHDOG_ACTION_RESET | WATCHDOG_ACTION_NMI

};
#endif

/*
 * Interfaces & driver glue
 */
static device_method_t cs5536_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_probe,		cs5536_probe),
	DEVMETHOD(device_attach,	cs5536_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

#ifdef GPIO
	/* GPIO interface */
	DEVMETHOD(gpio_pin_max,		cs5536_gpio_pin_max),
	DEVMETHOD(gpio_pin_set,		cs5536_gpio_pin_set),
	DEVMETHOD(gpio_pin_get,		cs5536_gpio_pin_get),
	DEVMETHOD(gpio_pin_toggle,	cs5536_gpio_pin_toggle),
	DEVMETHOD(gpio_pin_getcaps,	cs5536_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	cs5536_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getname,	cs5536_gpio_pin_getname),
	DEVMETHOD(gpio_pin_setflags,	cs5536_gpio_pin_setflags),
#endif

#ifdef SMBUS
	/* SMB interface */
	DEVMETHOD(smbus_intr,		cs5536_smbus_intr),
	DEVMETHOD(smbus_callback,	cs5536_smbus_callback),
	DEVMETHOD(smbus_quick,		cs5536_smbus_quick),
	DEVMETHOD(smbus_sendb,		cs5536_smbus_sendb),
	DEVMETHOD(smbus_recvb,		cs5536_smbus_recvb),
	DEVMETHOD(smbus_writeb,		cs5536_smbus_writeb),
	DEVMETHOD(smbus_writew,		cs5536_smbus_writew),
	DEVMETHOD(smbus_readb,		cs5536_smbus_readb),
	DEVMETHOD(smbus_readw,		cs5536_smbus_readw),
	DEVMETHOD(smbus_pcall,		cs5536_smbus_pcall),
	DEVMETHOD(smbus_bwrite,		cs5536_smbus_bwrite),
	DEVMETHOD(smbus_bread,		cs5536_smbus_bread),
#endif

#ifdef WATCHDOG
	/* Watchdog interface */
	DEVMETHOD(watchdog_enable,	cs5536_watchdog_enable),
	DEVMETHOD(watchdog_disable,	cs5536_watchdog_disable),
	DEVMETHOD(watchdog_configure,	cs5536_watchdog_configure),
	DEVMETHOD(watchdog_rearm,	cs5536_watchdog_rearm),
#endif

	/**/
	DEVMETHOD_END
};

static driver_t cs5536_driver =
{
	"cs5536",
	cs5536_methods,
	sizeof(struct cs5536_softc),
};

static devclass_t cs5536_devclass;

DRIVER_MODULE(cs5536, fdtbus, cs5536_driver, cs5536_devclass, 0, 0);

/*
 * Function definitions
 */
/* Device interface */
static int
cs5536_probe(device_t dev)
{

	device_set_desc(dev, "AMD CS5536");
	return 0;
}

static int
cs5536_attach(device_t dev)
{
	struct cs5536_softc *cs = device_get_softc(dev);
	device_t child;

#if 0
	/* watchdog */
	cs->cs_lbar_mfgpt = rdmsr(MSR_LBAR_MFGPT);

	device_add_child(dev, "gpiobus", -1);

	child = device_add_child(dev, "watchdog", -1);
	if (child == NULL)
		device_printf(dev, "failed to add watchdog");
	else
		device_set_ivars(child, &cs5536_watchdog_ivars);

	return bus_generic_attach(dev);
#endif
	return 0;
}

#ifdef GPIO
/* GPIO interface */
static int
cs5536_gpio_pin_max(device_t dev, int *npins)
{
	*npins = 32;

	return 0;
}

static int
cs5536_gpio_pin_set(device_t dev, uint32_t pin_num, uint32_t pin_value)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_get(device_t dev, uint32_t pin_num, uint32_t *pin_value)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_toggle(device_t dev, uint32_t pin_num)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_getcaps(device_t dev, uint32_t pin_num, uint32_t *caps)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_getflags(device_t dev, uint32_t pin_num, uint32_t *flags)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_getname(device_t dev, uint32_t pin_num, char *name)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_setflags(device_t dev, uint32_t pin_num, uint32_t flags)
{
	return ENOTSUP;
}
#endif

#ifdef smbus
/* SMB interface */
static void
cs5536_smbus_intr(device_t dev, u_char devaddr, char low, char high,
    int error)
{

	return ENOSYS;
}

static int
cs5536_smbus_callback(device_t dev, int index, void *data)
{

	return ENOSYS;
}

static int
cs5536_smbus_quick(device_t dev, u_char slave, int how)
{

	return ENOSYS;
}

static int
cs5536_smbus_sendb(device_t dev, u_char slave, char byte)
{

	return ENOSYS;
}

static int
cs5536_smbus_recvb(device_t dev, u_char slave, char *byte)
{

	return ENOSYS;
}

static int
cs5536_smbus_writeb(device_t dev, u_char slave, char cmd, char byte)
{

	return ENOSYS;
}

static int
cs5536_smbus_writew(device_t dev, u_char slave, char cmd, short word)
{

	return ENOSYS;
}

static int
cs5536_smbus_readb(device_t dev, u_char slave, char cmd, char *byte)
{

	return ENOSYS;
}

static int
cs5536_smbus_readw(device_t dev, u_char slave, char cmd, short *word)
{

	return ENOSYS;
}

static int
cs5536_smbus_pcall(device_t dev, u_char slave, char cmd, short sdata,
    short *rdata)
{

	return ENOSYS;
}

static int
cs5536_smbus_bwrite(device_t dev, u_char slave, char cmd, u_char count,
    char *buf)
{

	return ENOSYS;
}

static int
cs5536_smbus_bread(device_t dev, u_char slave, char cmd, u_char *count,
    char *buf)
{

	return ENOSYS;
}
#endif

#ifdef WATCHDOG
/* Watchdog interface */
static int
cs5536_watchdog_enable(device_t dev)
{
	struct cs5536_softc *cs = device_get_softc(dev);
	uint16_t val;

	/* Bits [12:0] are write once; check if the counter has been setup. */
	val = inw(cs->cs_lbar_mfgpt + MFGPT0_SETUP);
	if ((val & MFGPT_SETUP) == 0)
		return EINVAL;

	outw(cs->cs_lbar_mfgpt + MFGPT0_SETUP, MFGPT_CNT_EN);

	return 0;
}

static int
cs5536_watchdog_disable(device_t dev)
{
	struct cs5536_softc *cs = device_get_softc(dev);
	uint16_t val;

	/* Bits [12:0] are write once, check if the counter has been setup; */
	val = inw(cs->cs_lbar_mfgpt + MFGPT0_SETUP);
	if ((val & MFGPT_SETUP) && (val & MFGPT_CNT_EN) == 0)
		return EINVAL;

	/* Stop and reset counter */
	outw(cs->cs_lbar_mfgpt + MFGPT0_SETUP, 0);
	outw(cs->cs_lbar_mfgpt + MFGPT0_CNT, 0);

	return 0;
}

static int
cs5536_watchdog_configure(device_t dev, struct timespec *ts,
    watchdog_action_t action)
{
	struct cs5536_softc *cs = device_get_softc(dev);
	int count;
	uint32_t m;
	uint16_t val;

	count = ts->tv_sec * 2; /* XXX arg.... */

	val = inw(cs->cs_lbar_mfgpt + MFGPT0_SETUP);

	/* Bits [12:0] are write once, */
	if ((val & MFGPT_SETUP) == 0) {
		/* Set up MFGPT0, 32khz, prescaler 16k, C2 event */
		val = MFGPT_SCALE_16384;
		val |= MFGPT_CLKSEL_32K;
		val |= MFGPT_CMP2MODE(MFGPT_CMPMODE_EVENT);

		outw(cs->cs_lbar_mfgpt + MFGPT0_SETUP, val);
	}

	/* set comparator 2 */
	outw(cs->cs_lbar_mfgpt + MFGPT0_CMP2, count);

	/* reset counter */
	outw(cs->cs_lbar_mfgpt + MFGPT0_CNT, 0);

	/* Arm reset */
	m = rdmsr(MSR_MFTPT_NR);
	switch (action) {
	case WATCHDOG_ACTION_RESET:
		m &= ~(MSR_MFGPT0_C2_NMIM | NMI_LEG);
		m |= MSR_MFGPT0_C2_RSTEN;
		break;
	case WATCHDOG_ACTION_NMI:
		m &= ~MSR_MFGPT0_C2_RSTEN;
		m |= (MSR_MFGPT0_C2_NMIM | NMI_LEG);
		break;
	}
	wrmsr(MSR_MFTPT_NR, m);

	return 0;
}

static int
cs5536_watchdog_rearm(device_t dev)
{
	struct cs5536_softc *cs = device_get_softc(dev);

	outw(cs->cs_lbar_mfgpt + MFGPT0_CNT, 0);

	return 0;
}
#endif
