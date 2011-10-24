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
#include <sys/sbuf.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <dev/timecounter/timecountervar.h>

#include "timecounter_if.h"

/*
 * Prototypes
 */
/* Device interface */
static int timecounter_probe(device_t);
static int timecounter_attach(device_t);
static int timecounter_detach(device_t);

/*  */
static timecounter_get_t timecounter_get;

/*
 * Interfaces & driver glue
 */
static device_method_t timecounter_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_probe,		timecounter_probe),
	DEVMETHOD(device_attach,	timecounter_attach),
	DEVMETHOD(device_detach,	timecounter_detach),
	DEVMETHOD_END
};

devclass_t timecounter_devclass;

driver_t timecounter_driver =
{
	.name    = "timecounter",
	.methods = timecounter_methods,
};

/*
 * Function definitions
 */
/* Device interface */
static int
timecounter_probe(device_t dev)
{
	struct timecounter_ivars *ti = device_get_ivars(dev);

	KASSERT(ti != NULL, ("invalid ivars"));

	if (ti->ti_desc != NULL) {
		struct sbuf *sb = sbuf_new_auto();
		sbuf_printf(sb, "Timecounter - %s", ti->ti_desc);
		sbuf_finish(sb);

		device_set_desc_copy(dev, sbuf_data(sb));

		sbuf_delete(sb);
	} else {
		device_set_desc(dev, "Timecounter");
	}

	return 0;
}

static int
timecounter_attach(device_t dev)
{
	device_t parent = device_get_parent(dev);
	struct timecounter_softc *ts = device_get_softc(dev);
	struct timecounter_ivars *ti = device_get_ivars(dev);

	KASSERT(ts != NULL, ("invalid softc"));
	KASSERT(ti != NULL, ("invalid ivars"));

	TIMECOUNTER_INIT(parent);

	struct timecounter *tc = ti->ti_tc;

	tc->tc_get_timecount = timecounter_get;

	tc_init(tc);

	return 0;
}

static int
timecounter_detach(device_t dev)
{
	return 0;
}

/*  */
static u_int
timecounter_get(struct timecounter *tc)
{
	device_t dev = tc->tc_priv;

	return TIMECOUNTER_XGET(dev);
}
