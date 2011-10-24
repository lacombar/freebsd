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
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/led/led.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/timecounter/timecountervar.h>
#include <dev/watchdog/watchdogvar.h>

#include <machine/pc/bios.h>

#include "gpio_if.h"
#include "timecounter_if.h"
#include "watchdog_if.h"

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

/* xbus(4) */
#define CBA_WDTO	0x00
#define CBA_WDCNFG	0x02
#define CBA_WDSTS	0x04
#define CBA_TMVALUE	0x08
#define CBA_TMSTS 	0x0c
#define CBA_TMCNFG	0x0d

#define CBA_IID		0x3c
#define CBA_REV		0x3d

/* TMCNFG */
#define TM27MPD		(1<<2)		/* TIMER 27 MHz Power Down */
#define TMCLKSEL	(1<<1)		/* TIMER Clock Select */
#define TMEN		(1<<0)		/* TIMER Interrupt Enable */

/* WATCHDOG */
#define WD_EVENT_NONE			0
#define WD_EVENT_INTR			1
#define WD_EVENT_SMI			2
#define WD_EVENT_RESET			3

#define WD_EVENT_TYPE_1(ev)		((ev) << 4)
#define WD_EVENT_TYPE_2(ev)		((ev) << 6)

#define WD_PRESCALE_1			0x00
#define WD_PRESCALE_2			0x01
#define WD_PRESCALE_4			0x02
#define WD_PRESCALE_8			0x03
#define WD_PRESCALE_16			0x04
#define WD_PRESCALE_32			0x05
#define WD_PRESCALE_64			0x06
#define WD_PRESCALE_128			0x07
#define WD_PRESCALE_256			0x08
#define WD_PRESCALE_512			0x09
#define WD_PRESCALE_1024		0x0a
#define WD_PRESCALE_2048		0x0b
#define WD_PRESCALE_4096		0x0c
#define WD_PRESCALE_8192		0x0d

struct xbus_softc
{
	uint32_t	xs_cba;
	uint16_t	xs_wdt_timeout;
};

/*
 * Prototypxs declarations
 */
/* Device interface */
static int xbus_probe(device_t);
static int xbus_attach(device_t);

/* Timecounter interface */
static timecounter_init_t xbus_timecounter_init;
static timecounter_xget_t xbus_timecounter_get;

/* Watchdog interface */
static int xbus_watchdog_enable(device_t);
static int xbus_watchdog_disable(device_t);
static int xbus_watchdog_configure(device_t, struct timespec *,
	    watchdog_action_t);

/* Niscellaneous */
static uint64_t xbus_cputicks(void);

/*
 * Interfacxs & driver glue
 */
static device_method_t xbus_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_probe,		xbus_probe),
	DEVMETHOD(device_attach,	xbus_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Timecounter interface */
	DEVMETHOD(timecounter_init,	xbus_timecounter_init),
	DEVMETHOD(timecounter_xget,	xbus_timecounter_get),

	/* Watchdog interface */
	DEVMETHOD(watchdog_enable,	xbus_watchdog_enable),
	DEVMETHOD(watchdog_disable,	xbus_watchdog_disable),
	DEVMETHOD(watchdog_configure,	xbus_watchdog_configure),
	DEVMETHOD(watchdog_rearm,	xbus_watchdog_enable),

	/**/
	DEVMETHOD_END
};

static driver_t xbus_driver =
{
	"xbus",
	xbus_methods,
	sizeof(struct xbus_softc),
};

static devclass_t xbus_devclass;

DRIVER_MODULE(xbus, pci, xbus_driver, xbus_devclass, 0, 0);

/*
 * Local data
 */
/* Timecounter ivars */
static struct timecounter xbus_timecounter =
{
	.tc_counter_mask = 0xffffffff,
	.tc_frequency    = 27000000,
	.tc_name         = "SC1100",
	.tc_flags        = 1000
};

struct timecounter_ivars xbus_timecounter_ivars =
{
	.ti_tc = &xbus_timecounter
};

/* Watchdog ivars */
static struct watchdog_ivars xbus_watchdog_ivars =
{
	.wi_step    = WATCHDOG_TIMESPEC_STEP(0, (1000000000 / 32000) * 8192),
	.wi_nstep   = 16384,
	.wi_actions = WATCHDOG_ACTION_RESET
};

/**/
static device_t xbus_dev;

/*
 * Function definitions
 */
/* Device interface */
static int
xbus_probe(device_t dev)
{
	uint32_t devid;
	int error;

	devid = pci_get_devid(dev);
	if (devid != 0x0515100b) {
		error = ENXIO;
		goto out;
	}

	device_set_desc(dev, "AMD X-Bus Expansion");
	error = BUS_PROBE_DEFAULT;

out:
	return error;
}

static int
xbus_attach(device_t dev)
{
	struct xbus_softc *xs = device_get_softc(dev);
	device_t child;

	/* XXX al - need that for the cputicks() interface */
	xbus_dev = dev;

	/*
	 * The addrexs of the CBA is written to this register
	 * by the bios, see p161 in data sheet.
	 */
	xs->xs_cba = pci_read_config(dev, 0x64, 4);
	if (bootverbose) {
		device_printf(dev, "CBA at 0x%x\n", xs->xs_cba);
		device_printf(dev, "ID: %02x rev:%02x\n",
			inb(xs->xs_cba + CBA_IID),
			inb(xs->xs_cba + CBA_REV));
	}

	set_cputicker(xbus_cputicks, 27000000, 0);

	/* Add children */
	child = device_add_child(dev, "timecounter", 0);
	if (child != NULL)
		device_set_ivars(dev, &xbus_timecounter_ivars);

	child = device_add_child(dev, "watchdog", 0);
	if (child != NULL)
		device_set_ivars(dev, &xbus_watchdog_ivars);

	return bus_generic_attach(dev);
}

/* Timecounter interface */
static int
xbus_timecounter_init(device_t dev)
{
	struct xbus_softc *xs = device_get_softc(dev);

	outl(xs->xs_cba + CBA_TMCNFG, TMCLKSEL);

	return 0;
}

static u_int
xbus_timecounter_get(device_t dev)
{
	struct xbus_softc *xs = device_get_softc(dev);

	return inl(xs->xs_cba + CBA_TMVALUE);

}

/* Watchdog interface */
static int
xbus_watchdog_enable(device_t dev)
{
	struct xbus_softc *xs = device_get_softc(dev);

	outw(xs->xs_cba + CBA_WDTO, xs->xs_wdt_timeout);

	return 0;
}

static int
xbus_watchdog_disable(device_t dev)
{
	struct xbus_softc *xs = device_get_softc(dev);

	outw(xs->xs_cba + CBA_WDTO, 0);

	return 0;
}

static int
xbus_watchdog_configure(device_t dev, struct timespec *ts,
    watchdog_action_t action)
{
	struct xbus_softc *xs = device_get_softc(dev);
	uint16_t r;

	xs->xs_wdt_timeout = ts->tv_sec * 4; /* XXX arg.... */

	r = inw(xs->xs_cba + CBA_WDCNFG) & 0xff00;
	r |= WD_PRESCALE_8192;
	switch (action) {
	case WATCHDOG_ACTION_RESET:
		r |= WD_EVENT_TYPE_1(WD_EVENT_RESET);
		r |= WD_EVENT_TYPE_2(WD_EVENT_RESET);
		break;
	}

	outw(xs->xs_cba + CBA_WDCNFG, r);

	return 0;
}

/* Miscellaneous */
static uint64_t
xbus_cputicks()
{
	device_t dev = xbus_dev;
	struct xbus_softc *xs = device_get_softc(dev);
	uint32_t c;
	static unsigned last;
	static uint64_t offset;

	c = inl(xs->xs_cba + CBA_TMVALUE);
	if (c < last)
		offset += (1LL << 32);
	last = c;
	return (offset | c);
}



/* sc1100(4) */
enum sc1100_platform_type
{
	SC1100_SOEKRIS_NET48XX,
	SC1100_PCENGINES_WRAP,
};

struct sc1100_platform
{
	enum sc1100_platform_type	sp_type;
	struct bios_oem *		sp_bios;
};

struct sc1100_softc
{
	struct sc1100_platform *	ss_platform;
	uint32_t			ss_gpio;
};

/*
 * Local data
 */
static struct bios_oem bios_soekris_48 =
{
	{ 0xf0000, 0xf1000 },
	{
		{ "Soekris", 0, 8 },	/* Soekris Engineering. */
		{ "net4", 0, 8 },	/* net45xx */
		{ "comBIOS", 0, 54 },	/* comBIOS ver. 1.26a  20040819 ... */
		{ NULL, 0, 0 },
	}
};

static struct bios_oem bios_pcengines_wrap =
{
	{ 0xf9000, 0xfa000 },
	{
		{ "PC Engines WRAP", 0, 28 },	/* PC Engines WRAP.1C v1.03 */
		{ "tinyBIOS", 0, 28 },		/* tinyBIOS V1.4a (C)1997-2003 */
		{ NULL, 0, 0 },
	}
};

static struct sc1100_platform sc1100_platforms[] =
{
	{ SC1100_SOEKRIS_NET48XX, &bios_soekris_48 },
	{ SC1100_PCENGINES_WRAP,  &bios_pcengines_wrap },
};

/*
 * Prototypxs declarations
 */
/* Device interface */
static int sc1100_probe(device_t);
static int sc1100_attach(device_t);

/* GPIO interface */
static int sc1100_gpio_pin_max(device_t, int *);
static int sc1100_gpio_pin_set(device_t, uint32_t, uint32_t);
static int sc1100_gpio_pin_get(device_t, uint32_t, uint32_t *);
static int sc1100_gpio_pin_toggle(device_t, uint32_t);
static int sc1100_gpio_pin_getcaps(device_t, uint32_t, uint32_t *);
static int sc1100_gpio_pin_getflags(device_t, uint32_t, uint32_t *);
static int sc1100_gpio_pin_getname(device_t, uint32_t, char *);
static int sc1100_gpio_pin_setflags(device_t, uint32_t, uint32_t);

#ifdef notyet
/* Platform specific */
static int sc1100_soekris_48xx_attach(device_t);
static int sc1100_pcengines_wrap_attach(device_t);
#endif

/*
 * Interfacxs & driver glue
 */
static device_method_t sc1100_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_probe,		sc1100_probe),
	DEVMETHOD(device_attach,	sc1100_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* GPIO interface */
	DEVMETHOD(gpio_pin_max,		sc1100_gpio_pin_max),
	DEVMETHOD(gpio_pin_set,		sc1100_gpio_pin_set),
	DEVMETHOD(gpio_pin_get,		sc1100_gpio_pin_get),
	DEVMETHOD(gpio_pin_toggle,	sc1100_gpio_pin_toggle),
	DEVMETHOD(gpio_pin_getcaps,	sc1100_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	sc1100_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getname,	sc1100_gpio_pin_getname),
	DEVMETHOD(gpio_pin_setflags,	sc1100_gpio_pin_setflags),

	/**/
	DEVMETHOD_END
};

static driver_t sc1100_driver =
{
	"sc1100",
	sc1100_methods,
	sizeof(struct sc1100_softc),
};

static devclass_t sc1100_devclass;

DRIVER_MODULE(sc1100, pci, sc1100_driver, sc1100_devclass, 0, 0);
/*
 * Function definitions
 */
/* Device interface */
static int
sc1100_probe(device_t dev)
{
	uint32_t devid;
	int error;

	devid = pci_get_devid(dev);
	if (devid != 0x0510100b) {
		error = ENXIO;
		goto out;
	}

	device_set_desc(dev, "AMD SC1100");
	error = BUS_PROBE_DEFAULT;

out:
	return error;
}

static int
sc1100_attach(device_t dev)
{
	struct sc1100_softc *ss = device_get_softc(dev);
#define BIOS_OEM_MAXLEN 80
	static u_char bios_oem[BIOS_OEM_MAXLEN];
	int i;

	ss->ss_platform = NULL;
	for (i = 0; i < ARRAY_SIZE(sc1100_platforms); i++) {
		int ret;
		ret = bios_oem_strings(sc1100_platforms[i].sp_bios, bios_oem,
		    sizeof bios_oem);
		if (ret > 0) {
			ss->ss_platform = &sc1100_platforms[i];
			break;
		}
	}

	if (ss->ss_platform != NULL)
		device_printf(dev, "%s\n", bios_oem);

	ss->ss_gpio = pci_read_config(dev, PCIR_BAR(0), 4);
	ss->ss_gpio &= ~0x1f;

	if (bootverbose)
		device_printf(dev, "GPIO at 0x%.8x\n", ss->ss_gpio);
	
	return bus_generic_attach(dev);
}

/*
 * GPIO interface
 */
static int
sc1100_gpio_pin_max(device_t dev, int *npins)
{
	*npins = 32;

	return 0;
}

static int
sc1100_gpio_pin_set(device_t dev, uint32_t pin_num, uint32_t pin_value)
{
	return ENOTSUP;
}

static int
sc1100_gpio_pin_get(device_t dev, uint32_t pin_num, uint32_t *pin_value)
{
	return ENOTSUP;
}

static int
sc1100_gpio_pin_toggle(device_t dev, uint32_t pin_num)
{
	return ENOTSUP;
}

static int
sc1100_gpio_pin_getcaps(device_t dev, uint32_t pin_num, uint32_t *caps)
{
	return ENOTSUP;
}

static int
sc1100_gpio_pin_getflags(device_t dev, uint32_t pin_num, uint32_t *flags)
{
	return ENOTSUP;
}

static int
sc1100_gpio_pin_getname(device_t dev, uint32_t pin_num, char *name)
{
	return ENOTSUP;
}

static int
sc1100_gpio_pin_setflags(device_t dev, uint32_t pin_num, uint32_t flags)
{
	return ENOTSUP;
}

#ifdef notyet
/* Platform specific */
static int
sc1100_soekris_48xx_attach(device_t dev)
{
	/* 1 error LED on GPIO 20 */
	return 0;
}

static int
sc1100_pcengines_wrap_attach(device_t dev)
{
	/* 3 LEDs on GPIO 2, 3 and 18, active low */
	return 0;
}
#endif

