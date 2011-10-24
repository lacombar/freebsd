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
#include <sys/watchdog.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/led/led.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/watchdog/watchdogvar.h>

#include <machine/pc/bios.h>

#include "gpio_if.h"
#include "timecounter_if.h"
#include "watchdog_if.h"

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

/*
 * The Elan CPU can be run from a number of clock frequencies, this
 * allows you to override the default 33.3 MHZ.
 */
#ifndef CPU_ELAN_XTAL
#define CPU_ELAN_XTAL 33333333
#endif

/* WDTMRCTL */
#define WDTMRCTL_ENB		(1<<15)
#define WDTMRCTL_WRST_ENB	(1<<14)
#define WDTMRCTL_IRQ_FLG	(1<<12)

/* elan(4) */
enum elan_platform_type
{
	ELAN_SOEKRIS_NET45XX,
};

struct elan_platform
{
	enum elan_platform_type	ep_type;
	struct bios_oem *	ep_bios;
};

struct elan_softc
{
	struct elan_platform *	es_platform;
	struct elan_mmcr *	es_mmcr;
	struct pps_state	es_pps;
};

/*
 * Prototypes declarations
 */
/* Device interface */
static int elan_probe(device_t);
static int elan_attach(device_t);

/* Timecounter interface */
static timecounter_init_t elan_timecounter_init;
static timecounter_xget_t elan_timecounter_get;

/* Watchdog interface */
static int elan_watchdog_enable(device_t);
static int elan_watchdog_disable(device_t)
static int elan_watchdog_configure(device_t, struct timespec *,
	    watchdog_action_t);
static int elan_watchdog_rearm(device_t);

/*
 * Interfaces & driver glue
 */
static device_method_t elan_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_probe,		elan_probe),
	DEVMETHOD(device_attach,	elan_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Timecounter interface */
	DEVMETHOD(timecounter_xget,	elan_timecounter_get),

	/* Watchdog interface */
	DEVMETHOD(watchdog_enable,	elan_watchdog_enable),
	DEVMETHOD(watchdog_disable,	elan_watchdog_disable),
	DEVMETHOD(watchdog_configure,	elan_watchdog_configure),
	DEVMETHOD(watchdog_rearm,	elan_watchdog_rearm),

	/**/
	DEVMETHOD_END
};

driver_t elan_driver =
{
	"elan",
	elan_methods,
	sizeof(struct elan_softc),
};

devclass_t elan_devclass;

DRIVER_MODULE(elan, pci, elan_driver, elan_devclass, 0, 0);

static struct timecounter elan_timecounter =
{
	.tc_counter_mask = 0xffff,
	.tc_frequency    = CPU_ELAN_XTAL / 4,
	.tc_name         = "ELAN",
	.tc_flags        = 1000
};

struct timecounter_ivars elan_timecounter_ivar =
{
	.ti_tc = elan_timecounter;
}

/*
 * Local data
 */
static struct bios_oem bios_soekris_45 =
{
	{ 0xf0000, 0xf1000 },
	{
		{ "Soekris", 0, 8 },	/* Soekris Engineering. */
		{ "net4", 0, 8 },	/* net45xx */
		{ "comBIOS", 0, 54 },	/* comBIOS ver. 1.26a  20040819 ... */
		{ NULL, 0, 0 },
	}
};

static struct elan_platform elan_platforms[] =
{
	{ ELAN_SOEKRIS_NET45XX, &bios_soekris_45 },
};

/*
 * Function definitions
 */
/* Device interface */
static int
elan_probe(device_t dev)
{
	uint16_t devid;
	int i, error;

	devid = pci_get_devid(dev);
	if (devid != 0x30001022) {
		error = ENXIO;
		goto out;
	}

	device_set_desc(dev, "AMD Elan SC520");
	error = BUS_PROBE_DEFAULT;

out:
	return error;
}

static int
elan_attach(device_t dev)
{
	struct elan_softc *es = device_get_softc(dev);
#define BIOS_OEM_MAXLEN 80
	static u_char bios_oem[BIOS_OEM_MAXLEN] = "\0";
	device_t child;
	int i, error = 0;

	es->es_platform = NULL;
	for (i = 0; i < ARRAY_SIZE(elan_platforms); i++) {
		int ret;
		ret = bios_oem_strings(elan_platforms[i].ep_bios, bios_oem,
		    sizeof bios_oem);
		if (ret > 0) {
			es->es_platform = &elan_platforms[i];
			break;
		}
	}

	switch (pci_get_devid(dev)) {
	case 0x30001022:
		if (es->es_platform == NULL) {
			error = ENXIO;
			device_printf(dev, "unable to determine platform\n");
			goto fail;
		}
		device_printf(dev, "%s\n", bios_oem);
		break;
	}

	es->es_mmcr = pmap_mapdev(0xfffef000, 0x1000);

	/*-
	 * The i8254 is driven with a nonstandard frequency which is
	 * derived thusly:
	 *   f = 32768 * 45 * 25 / 31 = 1189161.29...
	 * We use the sysctl to get the i8254 (timecounter etc) into whack.
	 */

	new = 1189161;
	i = kernel_sysctlbyname(&thread0, "machdep.i8254_freq", 
	    NULL, 0, &new, sizeof new, NULL, 0);
	if (bootverbose || 1)
		printf("sysctl machdep.i8254_freq=%d returns %d\n", new, i);

	/* Add childeren */
	child = device_add_child(dev, "timecounter", 0);
	if (child != NULL)
		device_set_ivar(dev, &elan_timecounter_ivar);

	child = device_add_child(dev, "watchdog", 0);
	if (child != NULL)
		device_set_ivar(dev, &elan_timecounter_ivar);

	return bus_generic_attach(dev);
fail:
	return error;
}

/* Timecounter interface */
static int
elan_timecounter_init(device_t dev)
{
	struct elan_softc *es = device_get_softc(dev);
	volatile struct elan_mmcr *mmcr = es->es_mmcr;

	/* Start GP timer #2 and use it as timecounter, hz permitting */
	elan_mmcr->GPTMR2MAXCMPA = 0;
	elan_mmcr->GPTMR2CTL = 0xc001;

	return 0;
}

static u_int
elan_timecounter_get(device_t dev)
{
	struct elan_softc *es = device_get_softc(dev);
	volatile struct elan_mmcr *mmcr = es->es_mmcr;

	return mmcr->GPTMR2CNT;
}

/* Watchdog interface */
static int
elan_watchdog_enable(device_t dev)
{
	struct elan_softc *es = device_get_softc(dev);
	volatile struct elan_mmcr *mmcr = es->es_mmcr;
	int error = 0;
	uint16_t w;
	uint8_t w;

	w = mmcr->WDTMRCTL;
	if ((w & WDTMRCTL_ENB) != 0) {
		error = EINVAL;
		goto out;
	}

	b = mmcr->GPECHO;
	mmcr->GPECHO = 0;

	mmcr->WDTMRCTL = 0x3333;
	mmcr->WDTMRCTL = 0xcccc;
	mmcr->WDTMRCTL = w | WDTMRCTL_ENB;

	mmcr->GPECHO = b;

out:
	return error;
}

static int
elan_watchdog_disable(device_t dev)
{
	struct elan_softc *es = device_get_softc(dev);
	volatile struct elan_mmcr *mmcr = es->es_mmcr;
	int error = 0;
	uint16_t w;
	uint8_t w;

	w = mmcr->WDTMRCTL;
	if ((w & WDTMRCTL_ENB) == 0) {
		error = EINVAL;
		goto out;
	}

	b = mmcr->GPECHO;
	mmcr->GPECHO = 0;

	mmcr->WDTMRCTL = 0x3333;
	mmcr->WDTMRCTL = 0xcccc;
	mmcr->WDTMRCTL = (w & ~WDTMRCTL_ENB);

	mmcr->GPECHO = b;

out:
	return error;
}

static int
elan_watchdog_configure(device_t dev, struct timespec *ts,
    watchdog_action_t action)
{
	struct elan_softc *es = device_get_softc(dev);
	volatile struct elan_mmcr *mmcr = es->es_mmcr;
	uint16_t w;
	uint8_t b;
	int i;

	/* Watchdog should have been stopped before configuration */
	w = mmcr->WDTMRCTL;
	if ((w & WDTMRCTL_ENB) != 0) {
		error = EINVAL;
		goto out;
	}

	/*
	 * Only a discreet number of timeout can be configured. Possible
	 * value can be computed with the following formula:
	 *
	 * duration = 2^<exp> / CPU_ELAN_XTAL
	 * with <exp> [14] U [24;30]
	 *
	 * At 33.333MHz, we can set: 496us, 508ms (both ignored here),
	 * 1.02s, 2.03s, 4.07s, 8.173, 16.27s and 32.545s. The exponent
	 * ends up being written to the LSB of WDTMRCTL.
	 */
	for (i = 25; i < 31; i++) {
		int duration = (1 << i) / CPU_ELAN_XTAL;

		if (ts->tv_sec == duration)
			break;

		if (ts->tv_sec < duration)
			break;
	}

	i = imax(i, 30);
	i -= 23;

	w = 1 << i;
	switch (action) {
	case WATCHDOG_ACTION_RESET:
		w |= WDTMRCTL_WRST_ENB;
		break;
	}

	/*
	 * There is a bug in some silicon which prevents us from
	 * writing to the WDTMRCTL register if the GP echo mode is
	 * enabled.  GP echo mode on the other hand is desirable
	 * for other reasons.  Save and restore the GP echo mode
	 * around our hardware tom-foolery.
	 */
	b = elan_mmcr->GPECHO;
	elan_mmcr->GPECHO = 0;

	mmcr->WDTMRCTL = 0x3333;
	mmcr->WDTMRCTL = 0xcccc;
	mmcr->WDTMRCTL = w;

	mmcr->GPECHO = b;

	return 0;
}

static int
elan_watchdog_rearm(device_t dev)
{
	struct elan_softc *es = device_get_softc(dev);
	volatile struct elan_mmcr *mmcr = es->es_mmcr;
	uint8_t w;

	w = elan_mmcr->GPECHO;
	mmcr->GPECHO = 0;
	mmcr->WDTMRCTL = 0xaaaa;
	mmcr->WDTMRCTL = 0x5555;
	mmcr->GPECHO = w;

	return 0;
}
\
