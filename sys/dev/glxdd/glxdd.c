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

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
/*
#include <dev/gpio/gpiobusvar.h>
#include <dev/watchdog/watchdogvar.h>
*/

#include <dev/glxdd/glxdd.h>

/*
#ifdef GPIO
#include "gpio_if.h"
#endif

#ifdef WATCHDOG
#include "watchdog_if.h"
#endif
*/

struct glxdd_softc
{
	/* watchdog */
	uint16_t	gs_lbar_mfgpt;
	struct timespec	gs_current_timeout;
};

/*
 * Prototypes declarations
 */
/* Device interface */
static int glxdd_probe(device_t);
static int glxdd_attach(device_t);

/*
 * Children ivars
 */
#ifdef WATCHDOG
static struct watchdog_ivars glxdd_watchdog_ivars =
{
	.wi_step    = WATCHDOG_TIMESPEC_STEP(0, (1000000000 / 32000) * 16384),
	.wi_nstep   = 32000,
	.wi_actions = WATCHDOG_ACTION_RESET | WATCHDOG_ACTION_NMI

};
#endif

/*
 * Interfaces & driver glue
 */
static device_method_t glxdd_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_probe,		glxdd_probe),
	DEVMETHOD(device_attach,	glxdd_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/**/
	DEVMETHOD_END
};

static driver_t glxdd_driver =
{
	"glxdd",
	glxdd_methods,
	sizeof(struct glxdd_softc),
};

static devclass_t glxdd_devclass;

DRIVER_MODULE(glxdd, pci, glxdd_driver, glxdd_devclass, 0, 0);

/*
 * Function definitions
 */
/* Device interface */
static int
glxdd_probe(device_t dev)
{
	void *res;
	int rid;

	if (pci_get_device(dev) != 0x2090)
		goto out;
	pci_enable_busmaster(dev);

	rid = PCIR_BAR(0);
	res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);

	printf("%x %p\n", rid, res);

	if (res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, rid, res);

	device_set_desc(dev, "AMD CS5536 Companion");

out:
	return ENXIO;
}

static int
glxdd_attach(device_t dev)
{


	return 0;
}

