/*
 * Copyright (c) 2011, 2012 Arnaud Lacombe
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
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <isa/isareg.h>
#include <isa/isavar.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/hwmon/hwmonvar.h>

#include <dev/watchdog/watchdogvar.h>

#include "hwmon_if.h"

#include "watchdog_if.h"

#define NSSIO_ASSERT(cond)	/**/

#define __debugf(fmt, arg...)	printf("[%4.d] %s()" fmt, __LINE__, __func__, ##arg)
#define _debugf(fmt, arg...)	__debugf(": " fmt "\n", ##arg)
#define debugf(fmt, arg...)	_debugf(fmt, ##arg)
#define tracef()		__debugf("\n")

/*
 *
 */
#define PC87360_CHIP_ID			(0xE1)
#define PC87363_CHIP_ID			(0xE8)
#define PC87364_CHIP_ID			(0xE4)
#define PC87365_CHIP_ID			(0xE5)
#define PC87366_CHIP_ID			(0xE9)

/*
 *
 */
#define PC8736x_LDN		(0x07)
#define PC8736x_SID		(0x20)
#define PC8736x_SIOCF1		(0x21)
#define PC8736x_SIOCF2		(0x22)
#define PC8736x_SIOCF3		(0x23)
#define PC8736x_SIOCF4		(0x24)
#define PC8736x_SIOCF5		(0x25)
#define PC8736x_SRID		(0x27)
#define PC8736x_SIOCF8		(0x28)
#define PC8736x_SIOCFA		(0x2A)
#define PC8736x_SIOCFB		(0x2B)
#define PC8736x_SIOCFC		(0x2C)
#define PC8736x_SIOCFD		(0x2D)

/* Logical Device Activate Register */
#define PC8736x_LD_CONTROL		(0x30)
#define PC8736x_LD_CONTROL_ENABLE_BIT		(1<<0)

/* I/O Space Configuration Registers */
#define PC8736x_IO_CONF_BAR0_MSB	(0x60)
#define PC8736x_IO_CONF_BAR0_LSB	(0x61)
#define PC8736x_IO_CONF_BAR1_MSB	(0x62)
#define PC8736x_IO_CONF_BAR1_LSB	(0x63)

/* Interrupt Configuration Registers */
#define PC8736x_IRQ_CONFIG		(0x71)

/* DMA Configuration Registers */
#define PC8736x_DMA_CONF_CHAN_SELECT0	(0x74)
#define PC8736x_DMA_CONF_CHAN_SELECT1	(0x75)

/* Special Logical Device Configuration Registers */
#define PC8736x_LD_SPEC_CONFIG0		(0xF0)
#define PC8736x_LD_SPEC_CONFIG1		(0xF1)
#define PC8736x_LD_SPEC_CONFIG2		(0xF2)


/* Logical Device Numbers */
#define PC8736x_LDN0			(0x00)
#define PC8736x_LDN1			(0x01)
#define PC8736x_LDN2			(0x02)
#define PC8736x_LDN3			(0x03)
#define PC8736x_LDN4			(0x04)
#define PC8736x_LDN5			(0x05)
#define PC8736x_LDN6			(0x06)
#define PC8736x_LDN7			(0x07)
#define PC8736x_LDN8			(0x08)
#define PC8736x_LDN9			(0x09)
#define PC8736x_LDNA			(0x0A)
#define PC8736x_LDNB			(0x0B)
#define PC8736x_LDNC			(0x0C)
#define PC8736x_LDND			(0x0D)
#define PC8736x_LDNE			(0x0E)
#define PC8736x_LDN_MAX			(PC8736x_LDNE+1)


/* General-Purpose Input/Output */
#define PC8736x_GPIO_LDN		PC8736x_LDN7

#define PC8736x_GPIO_GPDO0		(0x00)
#define PC8736x_GPIO_GPDI0		(0x01)
#define PC8736x_GPIO_GPEVEN0		(0x02)
#define PC8736x_GPIO_GPEVST0		(0x03)
#define PC8736x_GPIO_GPDO1		(0x04)
#define PC8736x_GPIO_GPDI1		(0x05)
#define PC8736x_GPIO_GPEVST1		(0x06)
#define PC8736x_GPIO_GPDO2		(0x08)
#define PC8736x_GPIO_GPDI2		(0x09)
#define PC8736x_GPIO_GPDI3		(0x0B)
#define PC8736x_GPIO_GPDO3		(0x0A)
#define PC8736x_GPIO_GPDI3		(0x0B)

#define PC8736x_GPIO_REGS_MAX		PC8736x_GPIO_GPDI3
#define PC8736x_GPIO_MAP_SIZE		(PC8736x_GPIO_REGS_MAX + 1)

/* Access Bus Interface */
#define PC8736x_ACB_LDN			PC8736x_LDN8

#define PC8736x_ACB_ACBSDA		(0x00)
#define PC8736x_ACB_ACBST		(0x01)
#define PC8736x_ACB_ACBCST		(0x02)
#define PC8736x_ACB_ACBCTL1		(0x03)
#define PC8736x_ACB_ACBADDR		(0x04)
#define PC8736x_ACB_ACBCTL2		(0x05)

#define PC8736x_ACB_REGS_MAX		PC8736x_ACB_ACBCTL2
#define PC8736x_ACB_MAP_SIZE		(PC8736x_ACB_REGS_MAX + 1)

/* Fan Speed Control and Monitor */
#define PC8736x_FSCM_LDN		PC8736x_LDN9

#define PC8736x_FSCM_FCPSR0		(0x00)
#define PC8736x_FSCM_FCDCR0		(0x01)
#define PC8736x_FSCM_FCPSR1		(0x02)
#define PC8736x_FSCM_FCDCR1		(0x03)
#define PC8736x_FSCM_FCPSR2		(0x04)
#define PC8736x_FSCM_FCDCR2		(0x05)
#define PC8736x_FSCM_FMTHR0		(0x06)
#define PC8736x_FSCM_FMSPR0		(0x07)
#define PC8736x_FSCM_FMCSR0		(0x08)
#define PC8736x_FSCM_FMTHR1		(0x09)
#define PC8736x_FSCM_FMSPR1		(0x0A)
#define PC8736x_FSCM_FMCSR1		(0x0B)
#define PC8736x_FSCM_FMTHR2		(0x0C)
#define PC8736x_FSCM_FMSPR2		(0x0D)
#define PC8736x_FSCM_FMCSR2		(0x0E)

#define PC8736x_FSCM_REGS_MAX		PC8736x_FSCM_FMCSR2
#define PC8736x_FSCM_MAP_SIZE		(PC8736x_FSCM_REGS_MAX + 1)

/* Watchdog Timer */
#define PC8736x_WDT_LDN			PC8736x_LDNA

#define PC8736x_WDT_WDTO		(0x00)
#define PC8736x_WDT_WDMSK		(0x01)
#define PC8736x_WDT_WDST		(0x02)

#define PC8736x_WDT_REGS_MAX		PC8736x_WDT_WDST
#define PC8736x_WDT_MAP_SIZE		(PC8736x_WDT_REGS_MAX + 1)

/*
 * VLM and TMS are only available in the PC8736[56] variant of the chip.
 */

/* Voltage Level Monitor */

#define PC8736x_VLM_LDN			PC8736x_LDND

/* VLM Control and Status Register */
#define PC8736x_VLM_VEVSTS0		(0x00)
#define PC8736x_VLM_VEVSTS1		(0x01)
#define PC8736x_VLM_VEVSMI0		(0x02)
#define PC8736x_VLM_VEVSMI1		(0x03)
#define PC8736x_VLM_VEVIRQ0		(0x04)
#define PC8736x_VLM_VEVIRQ1		(0x05)
#define PC8736x_VLM_VID			(0x06)
#define PC8736x_VLM_VCNVR		(0x07)
#define PC8736x_VLM_VCNVR_CHAN_DELAY_40US	(0)
#define PC8736x_VLM_VCNVR_CHAN_DELAY_80US	(1<<3)
#define PC8736x_VLM_VCNVR_CHAN_DELAY_160US	(1<<4)
#define PC8736x_VLM_VCNVR_CHAN_PERIOD_2S	(0)
#define PC8736x_VLM_VCNVR_CHAN_PERIOD_1S_	(1)
#define PC8736x_VLM_VCNVR_CHAN_PERIOD_0_5S	(2)
#define PC8736x_VLM_VCNVR_CHAN_PERIOD_0_1S	(3)
#define PC8736x_VLM_VCNVR_CHAN_PERIOD_0_05S	(4)
#define PC8736x_VLM_VCNVR_CHAN_PERIOD_0_02S	(5)
#define PC8736x_VLM_VLMCFG		(0x08)
#define PC8736x_VLM_VLMCFG_INT_VREF		(0)
#define PC8736x_VLM_VLMCFG_EXT_VREF		(1<<0)
#define PC8736x_VLM_VLMCFG_MONITOR		(0)
#define PC8736x_VLM_VLMCFG_STANDBY		(1<<1)

#define PC8736x_VLM_VLMBS		(0x09)

#define PC8736x_VLM_COMMON_REG_MAX	PC8736x_VLM_VLMBS

/* VLM Channel Register Map */
#define PC8736x_VLM_VCHCFST		(0x0A)
#define PC8736x_VLM_VCHCFST_CHAN_ENABLE		(1<<0)
#define PC8736x_VLM_RDCHV		(0x0B)
#define PC8736x_VLM_CHVH		(0x0C)
#define PC8736x_VLM_CHVL		(0x0D)
#define PC8736x_VLM_OTSL		(0x0E)

#define PC8736x_VLM_CHANNEL_REG_MIN	PC8736x_VLM_VCHCFST
#define PC8736x_VLM_CHANNEL_REG_MAX	PC8736x_VLM_OTSL

#define PC8736x_VLM_REGS_MAX		PC8736x_VLM_OTSL
#define PC8736x_VLM_MAP_SIZE		(PC8736x_VLM_REGS_MAX + 1)

/* Temperature Sensor */

#define PC8736x_TMS_LDN		PC8736x_LDNE

/* VMS Control and Status Register */
#define PC8736x_TMS_TEVSTS		(0x00)
#define PC8736x_TMS_TEVSMI		(0x02)
#define PC8736x_TMS_TEVIRQ		(0x04)
#define PC8736x_TMS_TMSCFG		(0x08)
#define PC8736x_TMS_TMSBS		(0x09)

#define	PC8736x_TMS_COMMON_REG_MAX	PC8736x_TMS_TMSBS

/* TMS Channel Register Map */
#define PC8736x_TMS_TCHCFST		(0x0A)
#define PC8736x_TMS_RDCHT		(0x0B)
#define PC8736x_TMS_CHTH		(0x0C)
#define PC8736x_TMS_CHTL		(0x0D)
#define PC8736x_TMS_CHOTL		(0x0E)

#define PC8736x_TMS_CHANNEL_REG_MIN	PC8736x_TMS_TCHCFST
#define PC8736x_TMS_CHANNEL_REG_MAX	PC8736x_TMS_CHOTL

#define PC8736x_TMS_REGS_MAX		PC8736x_TMS_CHOTL
#define PC8736x_TMS_MAP_SIZE		(PC8736x_TMS_REGS_MAX + 1)

#define BIT_SET(field, bit)		((field) |= (bit))
#define BIT_CLEAR(field, bit)		((field) &= ~(bit))
#define BIT_ISSET(field, bit)		(((field) & (bit)) == (bit))

/*
 * Local types
 */
struct nssio_logical_device
{
	unsigned int		nld_enable:1;
	uint16_t		nld_base_address[2];
	size_t			nld_map_size;
	int			nld_rid;
	struct resource *	nld_res;
};

struct nssio_softc
{
	device_t		ns_dev;
	int			ns_rid;
	struct resource *	ns_res;
	uint8_t			ns_chip_id;
	uint8_t			ns_rev_id;
	struct nssio_logical_device ns_logical_devices[PC8736x_LDN_MAX];
};

typedef int (*logical_device_cb_t)(device_t, int, void *);

/*
 * Local data
 */
static size_t nssio_logical_device_map_sizes[] =
{
	[PC8736x_GPIO_LDN] = PC8736x_GPIO_MAP_SIZE,
	[PC8736x_ACB_LDN] = PC8736x_ACB_MAP_SIZE,
	[PC8736x_FSCM_LDN] = PC8736x_FSCM_MAP_SIZE,
	[PC8736x_WDT_LDN] = PC8736x_WDT_MAP_SIZE,
	[PC8736x_VLM_LDN] = PC8736x_VLM_MAP_SIZE,
	[PC8736x_TMS_LDN] = PC8736x_TMS_MAP_SIZE,
};

static struct hwmon_ivars nssio_hwmon_ivars;

static struct watchdog_ivars nssio_watchdog_ivars;

/*
 * Prototypes
 */
static void nssio_select_ldn(struct nssio_softc *, uint8_t);
static uint8_t nssio_read_reg(struct nssio_softc *, uint8_t);
static uint8_t nssio_read_ldn_reg(struct nssio_softc *, uint8_t, uint8_t);

static inline int for_each_logical_devices(device_t, logical_device_cb_t);
static int nssio_set_logical_device_resource(device_t, int, void *);
static int nssio_delete_logical_device_resource(device_t, int, void *);
static int nssio_alloc_logical_device_resource(device_t, int, void *);
static int nssio_release_logical_device_resource(device_t, int, void *);

static inline uint8_t nssio_vlm_read_reg(struct nssio_logical_device *, uint8_t);
static inline void nssio_vlm_write_reg(struct nssio_logical_device *, uint8_t, uint8_t);
static inline uint8_t nssio_vlm_read_channel_reg(struct nssio_logical_device *, uint8_t, uint8_t);
static inline void nssio_vlm_write_channel_reg(struct nssio_logical_device *, uint8_t, uint8_t, uint8_t);

static int nssio_init_vlm(device_t);

/* device(4) interface */
static device_attach_t		nssio_attach;
static device_detach_t		nssio_detach;
static device_probe_t		nssio_probe;
static device_identify_t	nssio_identify;

/* hwmon(4) interface */
static hwmon_in_get_min_t	nssio_in_get_min;
static hwmon_in_set_min_t	nssio_in_set_min;
static hwmon_in_get_max_t	nssio_in_get_max;
static hwmon_in_set_max_t	nssio_in_set_max;
static hwmon_in_get_input_t	nssio_in_get_input;

/* watchdog(4) interface */
static watchdog_enable_t	nssio_watchdog_enable;
static watchdog_disable_t	nssio_watchdog_disable;
static watchdog_configure_t	nssio_watchdog_configure;
static watchdog_rearm_t		nssio_watchdog_rearm;

/*
 *
 */
static devclass_t nssio_devclass;

static device_method_t nssio_methods[] =
{
	/* device(4) interface */
	DEVMETHOD(device_identify,	nssio_identify),
	DEVMETHOD(device_probe,		nssio_probe),
	DEVMETHOD(device_attach,	nssio_attach),
	DEVMETHOD(device_detach,	nssio_detach),

	/* hwmon(4) interface */
	DEVMETHOD(hwmon_in_get_min,	nssio_in_get_min),
	DEVMETHOD(hwmon_in_set_min,	nssio_in_set_min),
	DEVMETHOD(hwmon_in_get_max,	nssio_in_get_max),
	DEVMETHOD(hwmon_in_set_max,	nssio_in_set_max),
	DEVMETHOD(hwmon_in_get_input,	nssio_in_get_input),

	/* watchdog(4) interface */
	DEVMETHOD(watchdog_enable,	nssio_watchdog_enable),
	DEVMETHOD(watchdog_disable,	nssio_watchdog_disable),
	DEVMETHOD(watchdog_configure,	nssio_watchdog_configure),
	DEVMETHOD(watchdog_rearm,	nssio_watchdog_rearm),

	KOBJMETHOD_END
};

static driver_t nssio_driver =
{
	"nssio",
	nssio_methods,
	sizeof(struct nssio_softc)
};

DRIVER_MODULE(nssio, isa, nssio_driver, nssio_devclass, 0, 0);
DRIVER_MODULE(hwmon, nssio, hwmon_driver, hwmon_devclass, 0, 0);

MODULE_DEPEND(nssio, hwmon, 1, 1, 1);
MODULE_DEPEND(nssio, watchdog, 1, 1, 1);

/*
 *
 */
static void
nssio_select_ldn(struct nssio_softc *ns, uint8_t ldn)
{

	bus_write_1(ns->ns_res, 0, PC8736x_LDN);
	bus_write_1(ns->ns_res, 1, ldn);
}

static uint8_t
nssio_read_reg(struct nssio_softc *ns, uint8_t reg)
{
	uint8_t v;

	bus_write_1(ns->ns_res, 0, reg);
	v = bus_read_1(ns->ns_res, 1);

	return v;
}

static uint8_t
nssio_read_ldn_reg(struct nssio_softc *ns, uint8_t ldn, uint8_t reg)
{
	uint8_t v;

	nssio_select_ldn(ns, ldn);

	v = nssio_read_reg(ns, reg);

	return v;
}

/*
 *
 */
static int
for_each_logical_devices_arg(device_t dev, logical_device_cb_t cb, void *cb_arg)
{
	int error = 0, ldn;

	for (ldn = PC8736x_LDN0; ldn <= PC8736x_LDN_MAX; ldn++) {
		error = (*cb)(dev, ldn, cb_arg);
		if (error != 0)
			goto out;
	}

out:
	return error;
}

static inline int
for_each_logical_devices(device_t dev, logical_device_cb_t cb)
{

	return for_each_logical_devices_arg(dev, cb, NULL);
}

static int
nssio_set_logical_device_resource(device_t dev, int ldn, void *arg)
{
	struct nssio_softc *ns = device_get_softc(dev);
	struct nssio_logical_device *nld;
	uint16_t baddr0, baddr1;
	uint8_t ctrl;
	int error;
       
	error = 0;

	nld = &ns->ns_logical_devices[ldn];

	/* Skip if this logical device does not interest us. */
	nld->nld_map_size = nssio_logical_device_map_sizes[ldn];
	if (nld->nld_map_size == 0)
		goto out;

	/* Skip if this logical device is not enabled. */
	ctrl = nssio_read_ldn_reg(ns, ldn, PC8736x_LD_CONTROL);
	nld->nld_enable = (BIT_ISSET(ctrl, PC8736x_LD_CONTROL_ENABLE_BIT)) ? 1 : 0;
	if (!nld->nld_enable)
		goto out;

	baddr0 = nssio_read_ldn_reg(ns, ldn, PC8736x_IO_CONF_BAR0_MSB);
	baddr0 <<= 8;
	baddr0 |= nssio_read_ldn_reg(ns, ldn, PC8736x_IO_CONF_BAR0_LSB);
	nld->nld_base_address[0] = baddr0;

	baddr1 = nssio_read_ldn_reg(ns, ldn, PC8736x_IO_CONF_BAR1_MSB);
	baddr1 <<= 8;
	baddr1 |= nssio_read_ldn_reg(ns, ldn, PC8736x_IO_CONF_BAR1_LSB);
	nld->nld_base_address[1] = baddr1;

	nld->nld_rid = ldn + 1;

	error = bus_set_resource(dev, SYS_RES_IOPORT, nld->nld_rid,
	    nld->nld_base_address[0], nld->nld_map_size);
	if (error != 0) {
		nld->nld_rid = 0;
		goto out;
	}

out:
	return error;
}

static int
nssio_delete_logical_device_resource(device_t dev, int ldn, void *arg)
{
	/*
	 * FIXME al -
	 * Deleteing resource on isa(4) seem to always triggers:
	 *
	 * panic: resource_list_delete: resource has not been released
	 *
	 * even on resource properly released...
	 * To be investigated.
	 */
#ifdef notyet
	struct nssio_softc *ns = device_get_softc(dev);
	struct nssio_logical_device *nld;
	int error;
       
	error = 0;

	nld = &ns->ns_logical_devices[ldn];

	/* Did not interest us. */
	if (nld->nld_map_size == 0)
		goto out;

	/* Not enabled. */
	if (!nld->nld_enable)
		goto out;

	/* Not successfully set. */
	if (nld->nld_rid == 0)
		goto out;

	bus_delete_resource(dev, SYS_RES_IOPORT, ns->ns_rid);

out:
	return error;
#else
	return 0;
#endif
}

static int
nssio_alloc_logical_device_resource(device_t dev, int ldn, void *arg)
{
	struct nssio_softc *ns = device_get_softc(dev);
	struct nssio_logical_device *nld;
	int error = 0;
       
	nld = &ns->ns_logical_devices[ldn];
	
	/* Not reserved. */
	if (nld->nld_rid == 0)
		goto out;

	nld->nld_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &nld->nld_rid,
	    0, ~0, nld->nld_map_size, RF_ACTIVE);
	if (nld->nld_res == NULL) {
		error = EINVAL;
		goto out;
	}

out:
	return error;
}

static int
nssio_release_logical_device_resource(device_t dev, int ldn, void *arg)
{
	struct nssio_softc *ns = device_get_softc(dev);
	struct nssio_logical_device *nld;
	int error = 0;
       
	nld = &ns->ns_logical_devices[ldn];

	/* Not allocated. */
	if (nld->nld_res == NULL)
		goto out;

	bus_release_resource(dev, SYS_RES_IOPORT, nld->nld_rid,
	    nld->nld_res);

out:
	return error;
}

static inline uint8_t
nssio_vlm_read_reg(struct nssio_logical_device *nld, uint8_t reg)
{
	uint8_t v;

	NSSIO_ASSERT(reg <= PC8736x_VLM_COMMON_REG_MAX);

	v = bus_read_1(nld->nld_res, reg);

	return v;
}

static inline void
nssio_vlm_write_reg(struct nssio_logical_device *nld,
    uint8_t reg, uint8_t v)
{

	NSSIO_ASSERT(reg <= PC8736x_VLM_COMMON_REG_MAX);

	bus_write_1(nld->nld_res, reg, v);
}


static inline uint8_t
nssio_vlm_read_channel_reg(struct nssio_logical_device *nld, uint8_t channel,
    uint8_t reg)
{
	uint8_t v;

	NSSIO_ASSERT(channel < 11);
	NSSIO_ASSERT(reg <= PC8736x_VLM_REG_MAX);

	nssio_vlm_write_reg(nld, PC8736x_VLM_VLMBS, channel);
	v = bus_read_1(nld->nld_res, reg);

	return v;
}

static inline void
nssio_vlm_write_channel_reg(struct nssio_logical_device *nld, uint8_t channel,
    uint8_t reg, uint8_t v)
{

	NSSIO_ASSERT(channel < 11);
	NSSIO_ASSERT(reg <= PC8736x_VLM_CHANNEL_REG_MAX);
	NSSIO_ASSERT(reg >= PC8736x_VLM_CHANNEL_REG_MIN);

	nssio_vlm_write_reg(nld, PC8736x_VLM_VLMBS, channel);
	bus_write_1(nld->nld_res, reg, v);
}

/*
 *
 */
static int
nssio_init_vlm(device_t dev)
{
	struct nssio_softc *ns = device_get_softc(dev);
	struct nssio_logical_device *nld;
	int error, i;

	error = 0;

	nld = &ns->ns_logical_devices[PC8736x_VLM_LDN];
	printf("cfg -> %x\n", nssio_vlm_read_reg(nld, PC8736x_VLM_VLMCFG));

	/* Magic from page 208 */
	bus_write_1(nld->nld_res, 0x8, 0x00);
	bus_write_1(nld->nld_res, 0x9, 0x0f);
	bus_write_1(nld->nld_res, 0xa, 0x08);
	bus_write_1(nld->nld_res, 0xb, 0x04);
	bus_write_1(nld->nld_res, 0xc, 0x35);
	bus_write_1(nld->nld_res, 0xd, 0x05);
	bus_write_1(nld->nld_res, 0xe, 0x05);
	uint8_t v;

	v = PC8736x_VLM_VCNVR_CHAN_PERIOD_2S | PC8736x_VLM_VCNVR_CHAN_DELAY_160US;
	nssio_vlm_write_reg(nld, PC8736x_VLM_VCNVR, v);

	v = PC8736x_VLM_VLMCFG_MONITOR | PC8736x_VLM_VLMCFG_INT_VREF;
	nssio_vlm_write_reg(nld, PC8736x_VLM_VLMCFG, v);

	for (i = 0; i < 11; i++) {
		v = PC8736x_VLM_VCHCFST_CHAN_ENABLE;
		nssio_vlm_write_channel_reg(nld, i, PC8736x_VLM_VCHCFST, v);
	}

	return error;
}

/*
 * device(4) interface
 */
static void
nssio_identify(driver_t *driver, device_t parent)
{

	BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "nssio", 0);
}

static int
nssio_probe(device_t dev)
{
	struct nssio_softc *ns = device_get_softc(dev);
	int error = 0;

	error = bus_set_resource(dev, SYS_RES_IOPORT, 0, 0x2e, 2);
	if (error != 0)
		goto fail;

	ns->ns_rid = 0;
	ns->ns_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &ns->ns_rid,
	    0, ~0, 2, RF_ACTIVE);
	if (ns->ns_res == NULL) {
		error = ENXIO;
		goto fail;
	}

	ns->ns_chip_id = nssio_read_reg(ns, PC8736x_SID);
	switch (ns->ns_chip_id) {
	case PC87360_CHIP_ID:
		break;
	case PC87363_CHIP_ID:
		break;
	case PC87364_CHIP_ID:
		break;
	case PC87365_CHIP_ID:
		break;
	case PC87366_CHIP_ID:
		break;
	default:
		error = ENXIO;
		goto fail1;
	}

	ns->ns_rev_id = nssio_read_reg(ns, PC8736x_SRID);

	error = for_each_logical_devices(dev, nssio_set_logical_device_resource);
	if (error != 0)
		goto fail2;

	return 0;
fail2:
	for_each_logical_devices(dev, nssio_delete_logical_device_resource);
fail1:
	bus_release_resource(dev, SYS_RES_IOPORT, ns->ns_rid, ns->ns_res);
fail:
	return error;
}

static int
nssio_attach_hwmon(device_t dev)
{
	struct hwmon_ivars *hi = &nssio_hwmon_ivars;
	device_t child;
	int error = 0;

	hi->hi_in_nchannels = 11;

	child = device_add_child(dev, "hwmon", 0);
	if (child == NULL) {
		error = ENXIO;
		goto out;
	}

	device_set_ivars(child, hi);

out:
	return error;
}

static int
nssio_attach_watchdog(device_t dev)
{
	struct watchdog_ivars *wi = &nssio_watchdog_ivars;
	device_t child;
	int error = 0;

	child = device_add_child(dev, "watchdog", 0);
	if (child == NULL) {
		error = ENXIO;
		goto out;
	}

	device_set_ivars(child, wi);

out:
	return error;
}

static int
nssio_attach(device_t dev)
{
	struct nssio_softc *ns = device_get_softc(dev);
	int error;

	/* Allocate all interesting logical devices. */
	error = for_each_logical_devices(dev, nssio_alloc_logical_device_resource);
	if (error != 0)
		goto fail0;

	/* Initialize VLM */
	error = nssio_init_vlm(dev);
	if (error != 0)
		goto fail0;

	error = nssio_attach_hwmon(dev);
	if (error != 0)
		goto fail0;

	error = nssio_attach_watchdog(dev);
	if (error != 0)
		goto fail1;


	return bus_generic_attach(dev);
fail1:
	device_delete_children(dev);

fail0:
	(void)for_each_logical_devices(dev, nssio_release_logical_device_resource);

	(void)for_each_logical_devices(dev, nssio_delete_logical_device_resource);

	bus_release_resource(dev, SYS_RES_IOPORT, ns->ns_rid, ns->ns_res);
	return error;
}

static int
nssio_detach(device_t dev)
{
	struct nssio_softc *ns = device_get_softc(dev);
	int error = 0;

	device_delete_children(dev);

	(void)for_each_logical_devices(dev, nssio_release_logical_device_resource);

	(void)for_each_logical_devices(dev, nssio_delete_logical_device_resource);

	bus_release_resource(dev, SYS_RES_IOPORT, ns->ns_rid, ns->ns_res);

	return error;
}

/*
 * hwmon(4) interface
 */
static int
nssio_in_get_min_max(device_t dev, int channel, int min, int *valuep)
{
	struct nssio_softc *ns = device_get_softc(dev);
	struct nssio_logical_device *nld;
	int error = 0;
	uint8_t reg;

	nld = &ns->ns_logical_devices[PC8736x_VLM_LDN];

	reg = (min) ? PC8736x_VLM_CHVL : PC8736x_VLM_CHVH;

	*valuep = nssio_vlm_read_channel_reg(nld, channel, reg);

	return error;
}

static int
nssio_in_set_min_max(device_t dev, int channel, int min, int value)
{
	struct nssio_softc *ns = device_get_softc(dev);
	struct nssio_logical_device *nld;
	int error = 0;
	uint8_t reg;

	if (value > 255) {
		error = EINVAL;
		goto out;
	}

	nld = &ns->ns_logical_devices[PC8736x_VLM_LDN];

	reg = (min) ? PC8736x_VLM_CHVL : PC8736x_VLM_CHVH;

	nssio_vlm_write_channel_reg(nld, channel, reg, value);

out:
	return error;
}

#define __nssio_in_get_min(dev, channel, valuep)	\
	    nssio_in_get_min_max((dev), (channel), 1, (valuep))

#define __nssio_in_set_min(dev, channel, value)		\
	    nssio_in_set_min_max((dev), (channel), 1, (value))

#define __nssio_in_get_max(dev, channel, valuep)	\
	    nssio_in_get_min_max((dev), (channel), 0, (valuep))

#define __nssio_in_set_max(dev, channel, value)	\
	    nssio_in_set_min_max((dev), (channel), 0, (value))

static int
nssio_in_get_min(device_t dev, int channel, int *valuep)
{
	return __nssio_in_get_min(dev, channel, valuep);
}

static int
nssio_in_set_min(device_t dev, int channel, int value)
{

	return __nssio_in_set_min(dev, channel, value);
}

static int
nssio_in_get_max(device_t dev, int channel, int *valuep)
{

	return __nssio_in_get_max(dev, channel, valuep);
}

static int
nssio_in_set_max(device_t dev, int channel, int value)
{

	return __nssio_in_set_max(dev, channel, value);
}

static int
nssio_in_get_input(device_t dev, int channel, int *valuep)
{
	struct nssio_softc *ns = device_get_softc(dev);
	struct nssio_logical_device *nld;
	int error = 0;

	nld = &ns->ns_logical_devices[PC8736x_VLM_LDN];

	*valuep = nssio_vlm_read_channel_reg(nld, channel, PC8736x_VLM_RDCHV);

	return error;
}

/*
 * watchdog(4) interface
 */
static int
nssio_watchdog_enable(device_t dev)
{
	int error = 0;
	
	return error;
}

static int
nssio_watchdog_disable(device_t dev)
{
	int error = 0;

	return error;
}

static int
nssio_watchdog_configure(device_t dev, struct timespec *ts,
    watchdog_action_t action)
{
	int error = 0;

	return error;
}

static int
nssio_watchdog_rearm(device_t dev)
{
	int error = 0;

	return error;
}

