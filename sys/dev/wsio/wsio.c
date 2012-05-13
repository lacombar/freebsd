/*
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

/*
 *
 */


#define CR30			(0x30)
#define CR60			(0x60)
#define CR61			(0x61)
#define CR70			(0x70)

/* Global Control Registers */
#define W83627_GCR_SOFT_RESET		(0x00)
#define W83627_GCR_LDN			(0x07)
#define W83627_GCR_CHIP_ID_MSB		(0x20)
#define W83627_GCR_CHIP_ID_LSB		(0x21)
#define W83627_GCR_POWER_DOWN		(0x22)
#define W83627_GCR_IMM_POWER_DOWN	(0x23)
#define W83627_GCR_GLOB_OPT0		(0x24)
#define W83627_GCR_IF_TRI_EN		(0x25)
#define W83627_GCR_GLOB_OPT1		(0x26)
#define W83627_GCR_GLOB_OPT2		(0x28)
#define W83627_GCR_MF_PIN_SELECT0	(0x29)
#define W83627_GCR_SPI_CONFIG		(0x2A)
#define W83627_GCR_MF_PIN_SELECT1	(0x2C)
#define W83627_GCR_MF_PIN_SELECT2	(0x2D)
#define W83627_FDC		LDN_0

/* Logical Device Numbers */
#define LDN_0			(0x00)
#define LDN_1			(0x01)
#define LDN_2			(0x02)
#define LDN_3			(0x03)
#define LDN_4			(0x04)
#define LDN_5			(0x05)
#define LDN_6			(0x06)
#define LDN_7			(0x07)
#define LDN_8			(0x08)
#define LDN_9			(0x09)
#define LDN_A			(0x0A)
#define LDN_B			(0x0B)

/* Logical Device mapping */
#define W83627_LPT		LDN_1
#define W83627_UART_A		LDN_2
#define W83627_UART_B		LDN_3
#define W83627_KBC		LDN_5
#define W83627_GAME_PORT	LDN_7
#define W83627_MIDI_PORT	LDN_7
#define W83627_GPIO_PORT_1	LDN_7
#define W83627_GPIO_PORT_5	LDN_7
#define W83627_GPIO_PORT_2	LDN_8
#define W83627_GPIO_PORT_3	LDN_8
#define W83627_GPIO_PORT_4	LDN_9
#define W83627_ACPI		LDN_A
#define W83627_HARDWARE_MONITOR	LDN_B

/*
 *
 */
struct wsio_softc
{
	device_t	ws_dev;
	unsigned int	ws_efer;
};

#define W83627_EFIR(ws)		((ws)->ws_efer)
#define W83627_EFDR(ws)		((ws)->ws_efer + 1)

/*
 *
 */
static device_attach_t		wsio_attach;
static device_detach_t		wsio_detach;
static device_probe_t		wsio_probe;
static device_identify_t	wsio_identify;
static device_shutdown_t	wsio_shutdown;
static device_suspend_t		wsio_suspend;
static device_resume_t		wsio_resume;

/*
 *
 */
static devclass_t wsio_devclass;

static device_method_t wsio_methods[] =
{
	/* Device interface. */
	DEVMETHOD(device_probe,		wsio_probe),
	DEVMETHOD(device_attach,	wsio_attach),
	DEVMETHOD(device_detach,	wsio_detach),
	DEVMETHOD(device_identify,	wsio_identify),
	DEVMETHOD(device_shutdown,	wsio_shutdown),
	DEVMETHOD(device_suspend,	wsio_suspend),
	DEVMETHOD(device_resume,	wsio_resume),

	KOBJMETHOD_END
};

static driver_t wsio_driver =
{
	"wsio",
	wsio_methods,
	sizeof(struct wsio_softc)
};

DRIVER_MODULE(wsio, isa, wsio_driver, wsio_devclass, 0, 0);

/*
 *
 */
static inline void
wsio_write_eifr(struct wsio_softc *ws, uint8_t v)
{

	outb(W83627_EFIR(ws), v);
}

static inline void
wsio_write_efdr(struct wsio_softc *ws, uint8_t v)
{

	outb(W83627_EFDR(ws), v);
}

static inline uint8_t
wsio_read_efdr(struct wsio_softc *ws)
{

	return inb(W83627_EFDR(ws));
}

#define W83627_ENTER_EFM_MAGIC	0x87
#define W83627_EXIT_EFM_MAGIC	0xAA

static inline void
wsio_enter_efm(struct wsio_softc *ws)
{

	/* Must be done twice */
	wsio_write_eifr(ws, W83627_ENTER_EFM_MAGIC);
	wsio_write_eifr(ws, W83627_ENTER_EFM_MAGIC);
}

static inline void
wsio_exit_efm(struct wsio_softc *ws)
{

	wsio_write_eifr(ws, W83627_EXIT_EFM_MAGIC);
}

static void
wsio_config_ldn(struct wsio_softc *ws, uint8_t ldn)
{

	wsio_write_eifr(ws, W83627_GCR_LDN);
	wsio_write_efdr(ws, ldn);

}

static uint8_t
wsio_read_reg(struct wsio_softc *ws, uint8_t ldn, uint8_t reg)
{
	uint8_t v;

	wsio_enter_efm(ws);

	wsio_config_ldn(ws, ldn);

	wsio_write_eifr(ws, reg);
	v = wsio_read_efdr(ws);

	wsio_exit_efm(ws);

	return v;
}

static uint8_t
wsio_read_global_reg(struct wsio_softc *ws, uint8_t reg)
{
	uint8_t v;

	wsio_enter_efm(ws);

	wsio_write_eifr(ws, reg);
	v = wsio_read_efdr(ws);

	wsio_exit_efm(ws);

	return v;
}

#if 0
static void
wsio_write_reg(struct wsio_softc *ws, uint8_t ldn, uint8_t reg, uint8_t v)
{

	wsio_enter_efm(ws);

	wsio_config_reg(ws, ldn, reg);

	wsio_write_efdr(ws, v);

	wsio_exit_efm(ws);
}


static void
wsio_write_reg_bit(uint8_t ldn, uint8_t reg, uint8_t bit, uint8_t data)
{
	uint8_t tmp;

	tmp = read_wsio_reg(ldn, reg);
	tmp &= ~(bit);
	tmp |= data;
	write_wsio_reg(ldn, reg, tmp);
}
#endif

/*
 *
 */
#define W83627THF_CHIP_ID	(0x90)

#define W83627DHG_UHG_CHIP_ID	(0xc1)
#define W83627DHG_GCR_CHIP_ID	(0xb070)

#define W83627UHG_GCR_CHIP_ID	(0xa230)

/*
 * Device interface
 */
static void
wsio_identify(driver_t *driver, device_t parent)
{

	BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "wsio", 0);
}

static int
wsio_probe_chip_id(device_t dev, uint8_t *id)
{
	struct wsio_softc *ws = device_get_softc(dev);
	int error = 0;
	int base, addr, data, tmp;

	/* Retrieve the `Hardware Monitor base address' */
	tmp = wsio_read_reg(ws, W83627_HARDWARE_MONITOR, CR60);
	base = tmp << 8;

	tmp = wsio_read_reg(ws, W83627_HARDWARE_MONITOR, CR61);
	base |= tmp;

	/* Must be aligned on a 8-byte boundary */
	if ((base % 8) != 0) {
		error = EINVAL;
		goto out;
	}

	addr = base + 0x5;
	data = base + 0x6;

	outb(addr, 0x58);

	*id = inb(data);

out:
	return error;
}

static int
wsio_probe(device_t dev)
{
	struct wsio_softc *ws = device_get_softc(dev);
	int error, retried;
	uint8_t id;

	error = 0;
	retried = 0;

	ws->ws_efer = 0x2e;
retry:
	error = wsio_probe_chip_id(dev, &id);
	if (error != 0)
		goto fail;

	switch (id) {
	case W83627THF_CHIP_ID:
		break;
	case W83627DHG_UHG_CHIP_ID:
	    {
		uint16_t chip_id, ic_version;

		chip_id = wsio_read_global_reg(ws, W83627_GCR_CHIP_ID_MSB);
		chip_id <<= 8;
		chip_id |= wsio_read_global_reg(ws, W83627_GCR_CHIP_ID_LSB);

		/* IC version is the lowest nibble */
		ic_version = chip_id & 0xf;

		chip_id &= ~(0xf);

		switch (chip_id) {
		case W83627DHG_GCR_CHIP_ID:
			break;
		case W83627UHG_GCR_CHIP_ID:
			break;
		default:
			error = ENXIO;
			break;
		}
		break;
	    }
	default:
		error = ENXIO;
		break;
	};

	if (!retried && error != 0) {
		ws->ws_efer = 0x4e;
		retried = 1;
		goto retry;
	}

	return 0;

fail:
	return ENXIO;
}

static int
wsio_attach(device_t dev)
{
	struct wsio_softc *ws = device_get_softc(dev);
	int error = 0;

	(void)ws;

	return error;
}

static int
wsio_detach(device_t dev)
{
	struct wsio_softc *ws = device_get_softc(dev);
	int error = 0;

	(void)ws;

	return error;
}

static int
wsio_shutdown(device_t dev)
{
	struct wsio_softc *ws = device_get_softc(dev);
	int error = 0;

	(void)ws;

	return error;
}

static int
wsio_suspend(device_t dev)
{
	struct wsio_softc *ws = device_get_softc(dev);
	int error = 0;

	(void)ws;

	return error;
}

static int
wsio_resume(device_t dev)
{
	struct wsio_softc *ws = device_get_softc(dev);
	int error = 0;

	(void)ws;

	return error;
}
