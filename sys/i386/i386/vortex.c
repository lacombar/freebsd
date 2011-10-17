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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/gpio/gpiobusvar.h>

#include "gpio_if.h"

#define VORTEX_ID_PORT0		0xCF8
#define VORTEX_ID_PORT1		0xCFC

enum vortex_type
{
	VORTEX86_SX = 0x31504d44,
	VORTEX86_DX = 0x32504d44,
	VORTEX86_MX = 0x33504d44,
};
typedef enum vortex_type vortex_type_t;

struct vortex_softc
{
	vortex_type_t		vs_model;
};

/*
 * Prototypes
 */
/* Device interface */
static void vortex_identify(driver_t *, device_t);
static int vortex_probe(device_t);
static int vortex_attach(device_t);

/* GPIO interface */
static int vortex_gpio_pin_max(device_t, int *);
static int vortex_gpio_pin_set(device_t, uint32_t, uint32_t);
static int vortex_gpio_pin_get(device_t, uint32_t, uint32_t *);
static int vortex_gpio_pin_toggle(device_t, uint32_t);
static int vortex_gpio_pin_getcaps(device_t, uint32_t, uint32_t *);
static int vortex_gpio_pin_getflags(device_t, uint32_t, uint32_t *);
static int vortex_gpio_pin_getname(device_t, uint32_t, char *);
static int vortex_gpio_pin_setflags(device_t, uint32_t, uint32_t);

/*
 * Interfaces & driver glue
 */
static device_method_t vortex_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_identify,	vortex_identify),
	DEVMETHOD(device_probe,		vortex_probe),
	DEVMETHOD(device_attach,	vortex_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* GPIO interface */
	DEVMETHOD(gpio_pin_max,		vortex_gpio_pin_max),
	DEVMETHOD(gpio_pin_set,		vortex_gpio_pin_set),
	DEVMETHOD(gpio_pin_get,		vortex_gpio_pin_get),
	DEVMETHOD(gpio_pin_toggle,	vortex_gpio_pin_toggle),
	DEVMETHOD(gpio_pin_getcaps,	vortex_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	vortex_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getname,	vortex_gpio_pin_getname),
	DEVMETHOD(gpio_pin_setflags,	vortex_gpio_pin_setflags),

	/**/
	DEVMETHOD_END
};

driver_t vortex_driver =
{
	"vortex",
	vortex_methods,
	sizeof(struct vortex_softc),
};

devclass_t vortex_devclass;

DRIVER_MODULE(vortex, nexus, vortex_driver, vortex_devclass, 0, 0);

/*
 * Device interface
 */
static void
vortex_identify(driver_t *driver, device_t parent)
{

	if (BUS_ADD_CHILD(parent, 0, "vortex", 0) == NULL)
		panic(__func__);
}

static int
vortex_probe(device_t dev)
{
	struct vortex_softc *vs = device_get_softc(dev);
	char buf[32], *model;
	int error = 0;
	uint32_t val;

	outl(VORTEX_ID_PORT0, 0x80000090);
	val = inl(VORTEX_ID_PORT1);

	switch (val) {
	case VORTEX86_SX:
		model = "SX";
		break;
	case VORTEX86_DX:
		model = "DX";
		break;
	case VORTEX86_MX:
		model = "MX";
		break;
	default:
		error = ENXIO;
		goto out;
	}

	vs->vs_model = val;

	snprintf(buf, sizeof buf, "DMP Vortex%s", model);
	device_set_desc_copy(dev, buf);

out:
	return error;
}

static int
vortex_attach(device_t dev)
{
	device_t child;

	child = device_add_child(dev, "md6117d", -1);
	if (child == NULL)
		device_printf(dev, "unable to add md6117d(4)");

	return bus_generic_attach(dev);
}

/*
 * GPIO interface
 */
static int
vortex_gpio_pin_max(device_t dev, int *npins)
{
	*npins = 32;

	return 0;
}

static int
vortex_gpio_pin_set(device_t dev, uint32_t pin_num, uint32_t pin_value)
{
	return ENOTSUP;
}

static int
vortex_gpio_pin_get(device_t dev, uint32_t pin_num, uint32_t *pin_value)
{
	return ENOTSUP;
}

static int
vortex_gpio_pin_toggle(device_t dev, uint32_t pin_num)
{
	return ENOTSUP;
}

static int
vortex_gpio_pin_getcaps(device_t dev, uint32_t pin_num, uint32_t *caps)
{
	return ENOTSUP;
}

static int
vortex_gpio_pin_getflags(device_t dev, uint32_t pin_num, uint32_t *flags)
{
	return ENOTSUP;
}

static int
vortex_gpio_pin_getname(device_t dev, uint32_t pin_num, char *name)
{
	return ENOTSUP;
}

static int
vortex_gpio_pin_setflags(device_t dev, uint32_t pin_num, uint32_t flags)
{
	return ENOTSUP;
}

