/*
 * Copyright (c) 2012 Arnaud Lacombe
 * All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose hith or hithout fee is hereby granted, provided that the above
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
#include <sys/kdb.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/sbuf.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <dev/hwmon/hwmonvar.h>

#include "hwmon_if.h"

#define __debugf(cond, fmt, arg...)					\
({									\
	if (!!(cond)) {							\
		printf("[%4.d] %s()" fmt,				\
		    __LINE__, __func__, ##arg);				\
	}								\
})
#define _debugf(cond, fmt, arg...)	__debugf(cond, ": " fmt "\n", ##arg)
#define debugf(fmt, arg...)		_debugf(1, fmt, ##arg)
#define debugf_on(cond, fmt, arg...)	_debugf(cond, fmt, ##arg);
#define tracef()			__debugf(1, "\n")
#define tracef_on(cond)			__debugf(cond, "\n")

/*
 *
 */

#define __REGISTER_PROC(prefix, name, mandatory, access, fmt, desc)	\
({									\
	if (!!(mandatory) ||						\
	    HWMON_METHOD_IS_IMPLEMENTED(dev, prefix, name)) {		\
		__hwmon_register_sysctl_handler(dev, channel,		\
		    __STRING(prefix), __STRING(name),			\
		    __CONCAT(hwmon_,__CONCAT(prefix,__CONCAT(_,__CONCAT(name,_sysctl_handler)))), \
		    access, fmt, desc);					\
	}								\
})

#define REGISTER_MANDATORY_PROC_RO_INT(prefix, name)			\
    __REGISTER_PROC(prefix, name, 1, CTLTYPE_INT | CTLFLAG_RD, "I", "")

#define REGISTER_OPTIONAL_PROC_RO_INT(prefix, name)			\
    __REGISTER_PROC(prefix, name, 0, CTLTYPE_INT | CTLFLAG_RD, "I", "")

#define REGISTER_MANDATORY_PROC_RW_INT(prefix, name)			\
    __REGISTER_PROC(prefix, name, 1, CTLTYPE_INT | CTLFLAG_RW, "I", "")

#define REGISTER_OPTIONAL_PROC_RW_INT(prefix, name)			\
    __REGISTER_PROC(prefix, name, 0, CTLTYPE_INT | CTLFLAG_RW, "I", "")

#define REGISTER_MANDATORY_PROC_RW_STR(prefix, name)			\
    __REGISTER_PROC(prefix, name, 1, CTLTYPE_STRING | CTLFLAG_RW, "A", "")

#define REGISTER_OPTIONAL_PROC_RO_STR(prefix, name)			\
    __REGISTER_PROC(prefix, name, 0, CTLTYPE_STRING | CTLFLAG_RD, "A", "")

/*
 *
 */
struct hwmon_softc
{
};

typedef int (*hwmon_sysctl_handler_t)(SYSCTL_HANDLER_ARGS);

typedef int (*hwmon_sysctl_get_int_handler_t)(device_t, int, int *);
typedef int (*hwmon_sysctl_set_int_handler_t)(device_t, int, int);

typedef int (*hwmon_sysctl_get_string_handler_t)(device_t, int, const char **);
typedef int (*hwmon_sysctl_set_string_handler_t)(device_t, int, const char *);

/*
 * Prototypes
 */
static kobjop_desc_t hwmon_in_lookup_kobj_desc(struct hwmon_kobj_method *, const char *);
static int __hwmon_method_is_implemented(device_t, struct hwmon_kobj_method *, const char *, const char *)

static inline int __hwmon_in_method_is_implemented(device_t, const char *, const char *);
static inline int hwmon_in_method_is_implemented(device_t, const char *);

static int hwmon_sysctl_int_handler(SYSCTL_HANDLER_ARGS, hwmon_sysctl_get_int_handler_t, hwmon_sysctl_set_int_handler_t);
static int hwmon_sysctl_string_handler(SYSCTL_HANDLER_ARGS, hwmon_sysctl_get_string_handler_t, hwmon_sysctl_set_string_handler_t);

static int __hwmon_register_sysctl_handler(device_t, int, const char *, const char *, hwmon_sysctl_handler_t, int, const char *, const char *);

static int hwmon_register_in_sysctl(device_t);
static int hwmon_register_sysctl(device_t);

static int hwmon__get_name(device_t, int, const char **);
static int hwmon__get_update_interval(device_t, int, int);
static int hwmon__set_update_interval(device_t, int, int *);

/* device(4) interface */
static device_probe_t	hwmon_probe;
static device_attach_t	hwmon_attach;
static device_detach_t	hwmon_detach;

/* hwmon(4) interface */
static hwmon_get_name_t			hwmon_get_name;
static hwmon_get_update_interval_t	hwmon_get_update_interval;
static hwmon_set_update_interval_t	hwmon_set_update_interval;

static hwmon_in_get_label_t	hwmon_in_get_label;
static hwmon_in_get_min_t	hwmon_in_get_min;
static hwmon_in_set_min_t	hwmon_in_set_min;
static hwmon_in_get_lcrit_t	hwmon_in_get_lcrit;
static hwmon_in_set_lcrit_t	hwmon_in_set_lcrit;
static hwmon_in_get_max_t	hwmon_in_get_max;
static hwmon_in_set_max_t	hwmon_in_set_max;
static hwmon_in_get_crit_t	hwmon_in_get_crit;
static hwmon_in_set_crit_t	hwmon_in_set_crit;
static hwmon_in_get_input_t	hwmon_in_get_input;
static hwmon_in_get_average_t	hwmon_in_get_average;
static hwmon_in_get_lowest_t	hwmon_in_get_lowest;
static hwmon_in_get_highest_t	hwmon_in_get_highest;
static hwmon_in_reset_history_t	hwmon_in_reset_history;

/*
 * Local data
 */
static device_method_t hwmon_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_probe,		hwmon_probe),
	DEVMETHOD(device_attach,	hwmon_attach),
	DEVMETHOD(device_detach,	hwmon_detach),
	DEVMETHOD_END
};

struct hwmon_kobj_method
{
	const char *	hkm_name;
	kobjop_desc_t	hkm_kobj_desc;
};

#define HWMON_METHOD(name)	{ __STRING(name), &name##_desc }

#define HWMON_METHOD_END	{ NULL, NULL }

static struct hwmon_kobj_method hwmon_in_kobj_methods[] =
{
	HWMON_METHOD(hwmon_in_get_label),
	HWMON_METHOD(hwmon_in_get_min),
	HWMON_METHOD(hwmon_in_set_min),
	HWMON_METHOD(hwmon_in_get_lcrit),
	HWMON_METHOD(hwmon_in_set_lcrit),
	HWMON_METHOD(hwmon_in_get_max),
	HWMON_METHOD(hwmon_in_set_max),
	HWMON_METHOD(hwmon_in_get_crit),
	HWMON_METHOD(hwmon_in_set_crit),
	HWMON_METHOD(hwmon_in_get_input),
	HWMON_METHOD(hwmon_in_get_average),
	HWMON_METHOD(hwmon_in_get_lowest),
	HWMON_METHOD(hwmon_in_get_highest),
	HWMON_METHOD(hwmon_in_reset_history),
	HWMON_METHOD_END
};

MODULE_VERSION(hwmon, 1);

/*
 * Public data
 */
devclass_t hwmon_devclass;

driver_t hwmon_driver =
{
	.name    = "hwmon",
	.methods = hwmon_methods,
	.size    = sizeof(struct hwmon_softc),
};

/*
 * Helpers inlines
 */

/*
 * Function definitions
 */

static kobjop_desc_t
hwmon_in_lookup_kobj_desc(struct hwmon_kobj_method *hkm0, const char *name)
{
	struct hwmon_kobj_method *hkm;
	kobjop_desc_t kd = NULL;

	for (hkm = hkm0; hkm->hkm_name != NULL; hkm++) {
		if (strcmp(name, hkm->hkm_name) == 0) {
			kd = hkm->hkm_kobj_desc;
			break;
		}
	}

	return kd;
}

static int
__hwmon_method_is_implemented(device_t dev, struct hwmon_kobj_method *hkm,
    const char *prefix, const char *name)
{
	device_t parent = device_get_parent(dev);
	kobj_t kobj = (kobj_t)parent;
	kobjop_desc_t kd;
	struct sbuf *sb;

	sb = sbuf_new_auto();

	sbuf_printf(sb, "hwmon_%s_%s", prefix, name);
	sbuf_finish(sb);

	kd = hwmon_in_lookup_kobj_desc(hkm, sbuf_data(sb));

	sbuf_delete(sb);

	return (kd != NULL &&
	    kobjop_lookup(kobj->ops, kd) != kobj_error_method);
}

static inline int
__hwmon_in_method_is_implemented(device_t dev, const char *prefix,
    const char *name)
{

	return __hwmon_method_is_implemented(dev, hwmon_in_kobj_methods,
	    prefix, name);
}

static inline int
hwmon_in_method_is_implemented(device_t dev, const char *name)
{

	return __hwmon_in_method_is_implemented(dev, "in", name) ||
	    __hwmon_in_method_is_implemented(dev, "in_get", name) ||
	    __hwmon_in_method_is_implemented(dev, "in_set", name);
}

#define HWMON_IN_METHOD_IS_IMPLEMENTED(dev, name)		\
	    hwmon_in_method_is_implemented(dev, __STRING(name))

/*
 * Voltage helpers
 */
static int
hwmon_sysctl_int_handler(SYSCTL_HANDLER_ARGS,
    hwmon_sysctl_get_int_handler_t get_cb,
    hwmon_sysctl_set_int_handler_t set_cb)
{
	device_t dev = oidp->oid_arg1;
	int channel = oidp->oid_arg2;
	int v, error = 0;

	if (set_cb && req->newptr != NULL) {
		v = *(int *)req->newptr;
		error = (*set_cb)(dev, channel, v);
		if (error != 0)
			goto out;
	}

	error = (*get_cb)(dev, channel, &v);
	if (error != 0)
		goto out;

	error = sysctl_handle_int(oidp, &v, 0, req);

out:
	return error;
}

#define HWMON_SYSCTL_INT_HANDLER(prefix, name, get_cb, set_cb)	\
static int								\
__CONCAT(hwmon_,__CONCAT(prefix,__CONCAT(_,__CONCAT(name,_sysctl_handler))))(SYSCTL_HANDLER_ARGS) \
{									\
	int error;							\
									\
	error = hwmon_sysctl_int_handler(oidp, arg1, arg2, req,		\
	    (get_cb),							\
	    (set_cb));							\
									\
	return error;							\
}

#define HWMON_SYSCTL_INT_HANDLER_RW(prefix, name)			\
	HWMON_SYSCTL_INT_HANDLER(prefix, name,				\
	    __CONCAT(hwmon_,__CONCAT(prefix,__CONCAT(_get_,name))),	\
	    __CONCAT(hwmon_,__CONCAT(prefix,__CONCAT(_set_,name))))

#define HWMON_SYSCTL_INT_HANDLER_RO(prefix, name)			\
	HWMON_SYSCTL_INT_HANDLER(prefix, name,				\
	    __CONCAT(hwmon_,__CONCAT(prefix,__CONCAT(_get_,name))),	\
	    NULL)

static int
hwmon_sysctl_string_handler(SYSCTL_HANDLER_ARGS,
    hwmon_sysctl_get_string_handler_t get_cb,
    hwmon_sysctl_set_string_handler_t set_cb)
{
	device_t dev = oidp->oid_arg1;
	int channel = oidp->oid_arg2;
	const char *p;
	int error = 0;

	error = (*get_cb)(dev, channel, &p);
	if (error != 0)
		goto out;

#define __UNCONST(p)	((void *)((uintptr_t)p))

	error = sysctl_handle_string(oidp, __UNCONST(p), strlen(p), req);

#undef __UNCONST

out:
	return error;
}

#define HWMON_SYSCTL_STRING_HANDLER(prefix, name, get_cb, set_cb)	\
static int								\
__CONCAT(hwmon_,__CONCAT(prefix,__CONCAT(_,__CONCAT(name,_sysctl_handler))))(SYSCTL_HANDLER_ARGS) \
{									\
	int error;							\
									\
	error = hwmon_sysctl_string_handler(oidp, arg1, arg2, req,	\
	    (get_cb),							\
	    (set_cb));							\
									\
	return error;							\
}

#define HWMON_SYSCTL_STRING_HANDLER_RO(prefix, name)			\
	HWMON_SYSCTL_STRING_HANDLER(prefix, name,			\
	    __CONCAT(hwmon_,__CONCAT(prefix,__CONCAT(_get_,name))),	\
	    NULL)


/* sysctl(4) handlers */
HWMON_SYSCTL_STRING_HANDLER_RO(,name)

HWMON_SYSCTL_INT_HANDLER_RW(,update_interval)

/*   Voltages */
HWMON_SYSCTL_STRING_HANDLER_RO(in, label)

HWMON_SYSCTL_INT_HANDLER_RW(in, min)
HWMON_SYSCTL_INT_HANDLER_RW(in, lcrit)
HWMON_SYSCTL_INT_HANDLER_RW(in, max)
HWMON_SYSCTL_INT_HANDLER_RW(in, crit)
                           
HWMON_SYSCTL_INT_HANDLER_RO(in, input)
HWMON_SYSCTL_INT_HANDLER_RO(in, average)
HWMON_SYSCTL_INT_HANDLER_RO(in, lowest)
HWMON_SYSCTL_INT_HANDLER_RO(in, highest)

static int
hwmon_in_reset_history_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	device_t dev = oidp->oid_arg1;
	int channel = oidp->oid_arg2;
	int error = 0;

	if (req->newptr != NULL) {
		int new = *(int *)req->newptr;

		if (new != 1) {
			error = EINVAL;
			goto out;
		}

		error = hwmon_in_reset_history(dev, channel);
		if (error != 0)
			goto out;
	}

out:
	return error;
}

/* sysctl(4) registration */
static int
__hwmon_register_sysctl_handler(device_t dev, int channel,
    const char *prefix, const char *name,
    hwmon_sysctl_handler_t handler, int access, const char *fmt,
    const char *desc)
{
	struct sysctl_oid_list *child;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sbuf *sb;
	int error = 0;

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	sb = sbuf_new_auto();

	sbuf_printf(sb, "%s%d_%s", prefix, channel, name);
	sbuf_finish(sb);

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, sbuf_data(sb), access, dev,
	    channel, handler, fmt, desc);

	sbuf_delete(sb);

	return error;
}

static int
hwmon_register_in_sysctl(device_t dev)
{
	struct hwmon_ivars *hi = device_get_ivars(dev);
	int channel, error = 0;

	for (channel = 0; channel < hi->hi_in_nchannels; channel++) {
		REGISTER_OPTIONAL_PROC_RO_STR(in, label);

		REGISTER_OPTIONAL_PROC_RW_INT(in, min);
		REGISTER_OPTIONAL_PROC_RW_INT(in, lcrit);
		REGISTER_OPTIONAL_PROC_RW_INT(in, max);
		REGISTER_OPTIONAL_PROC_RW_INT(in, crit);

		REGISTER_MANDATORY_PROC_RO_INT(in, input);

		REGISTER_OPTIONAL_PROC_RO_INT(in, average);
		REGISTER_OPTIONAL_PROC_RO_INT(in, lowest);
		REGISTER_OPTIONAL_PROC_RO_INT(in, highest);

		REGISTER_OPTIONAL_PROC_RW_INT(in, reset_history);
	}

	return error;
}

static int
hwmon_register_sysctl(device_t dev)
{
	int error = 0;

	error = hwmon_register_in_sysctl(dev);
	if (error != 0)
		goto out;

out:
	return error;
}


static int
hwmon__get_name(device_t dev, int dummy, const char **p)
{

	return hwmon_get_name(dev, p);
}

static int
hwmon__get_update_interval(device_t dev, int dummy, int *valuep)
{

	return hwmon_get_update_interval(dev, valuep);
}

static int
hwmon__get_update_interval(device_t dev, int dummy, int value)
{

	return hwmon_set_update_interval(dev, value);
}

/* device(4) interface */
static int
hwmon_probe(device_t dev)
{

	return 0;
}

static int
hwmon_attach(device_t dev)
{
	struct hwmon_ivars *hi = device_get_ivars(dev);
	int error = 0;

	if (hi == NULL) {
		error = EINVAL;
		goto out;
	}

	error = hwmon_register_sysctl(dev);
	if (error != 0)
		goto out;

out:
	return error;
}

static int
hwmon_detach(device_t dev)
{

	return 0;
}

/* hwmon(4) interface */
/*
 * Common helper
 */
static int
hwmon_get_name(device_t dev, const char **namep)
{
	device_t parent = device_get_parent(dev);

	return HWMON_GET_NAME(parent, namep);
}

static int
hwmon_get_update_interval(device_t dev, int *intervalp)
{
	device_t parent = device_get_parent(dev);

	return HWMON_GET_UPDATE_INTERVAL(parent, intervalp);
}

static int
hwmon_set_update_interval(device_t dev, int interval)
{
	device_t parent = device_get_parent(dev);

	return HWMON_SET_UPDATE_INTERVAL(parent, interval);
}

/*
 * Voltage helper
 */
static int
hwmon_in_get_label(device_t dev, int channel, const char **labelp)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_GET_LABEL(parent, channel, labelp);
}

static int
hwmon_in_get_min(device_t dev, int channel, int *valuep)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_GET_MIN(parent, channel, valuep);
}

static int
hwmon_in_set_min(device_t dev, int channel, int value)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_SET_MIN(parent, channel, value);
}

static int
hwmon_in_get_lcrit(device_t dev, int channel, int *valuep)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_GET_LCRIT(parent, channel, valuep);
}

static int
hwmon_in_set_lcrit(device_t dev, int channel, int value)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_SET_LCRIT(parent, channel, value);
}

static int
hwmon_in_get_max(device_t dev, int channel, int *valuep)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_GET_MAX(parent, channel, valuep);
}

static int
hwmon_in_set_max(device_t dev, int channel, int value)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_SET_MAX(parent, channel, value);
}

static int
hwmon_in_get_crit(device_t dev, int channel, int *valuep)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_GET_CRIT(parent, channel, valuep);
}

static int
hwmon_in_set_crit(device_t dev, int channel, int value)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_SET_CRIT(parent, channel, value);
}

static int
hwmon_in_get_input(device_t dev, int channel, int *valuep)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_GET_INPUT(parent, channel, valuep);
}

static int
hwmon_in_get_average(device_t dev, int channel, int *valuep)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_GET_AVERAGE(parent, channel, valuep);
}

static int
hwmon_in_get_lowest(device_t dev, int channel, int *valuep)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_GET_LOWEST(parent, channel, valuep);
}

static int
hwmon_in_get_highest(device_t dev, int channel, int *valuep)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_GET_HIGHEST(parent, channel, valuep);
}

static int
hwmon_in_reset_history(device_t dev, int channel)
{
	device_t parent = device_get_parent(dev);

	return HWMON_IN_RESET_HISTORY(parent, channel);
}

#if 0
#include <isa/isareg.h>
#include <isa/isavar.h>

/* dummy_hwmon(4) */
struct dummy_hwmon_softc
{
};

static device_identify_t	dummy_hwmon_identify;
static device_probe_t	dummy_hwmon_probe;
static device_attach_t	dummy_hwmon_attach;
static device_detach_t	dummy_hwmon_detach;

static hwmon_in_get_label_t dummy_hwmon_in_get_label;
static hwmon_in_get_min_t dummy_hwmon_in_get_min;
static hwmon_in_set_min_t dummy_hwmon_in_set_min;
static hwmon_in_get_max_t dummy_hwmon_in_get_max;
static hwmon_in_set_max_t dummy_hwmon_in_set_max;
static hwmon_in_get_input_t dummy_hwmon_in_get_input;

static device_method_t dummy_hwmon_methods[] =
{
	/* device(4) interface */
	DEVMETHOD(device_identify,	dummy_hwmon_identify),
	DEVMETHOD(device_probe,		dummy_hwmon_probe),
	DEVMETHOD(device_attach,	dummy_hwmon_attach),
	DEVMETHOD(device_detach,	dummy_hwmon_detach),

	/* hwmon(4) interface */
	DEVMETHOD(hwmon_in_get_label,	dummy_hwmon_in_get_label),
	DEVMETHOD(hwmon_in_get_min,	dummy_hwmon_in_get_min),
	DEVMETHOD(hwmon_in_set_min,	dummy_hwmon_in_set_min),
	DEVMETHOD(hwmon_in_get_max,	dummy_hwmon_in_get_max),
	DEVMETHOD(hwmon_in_set_max,	dummy_hwmon_in_set_max),
	DEVMETHOD(hwmon_in_get_input,	dummy_hwmon_in_get_input),

	DEVMETHOD_END
};

devclass_t dummy_hwmon_devclass;

driver_t dummy_hwmon_driver =
{
	.name    = "dummy_hwmon",
	.methods = dummy_hwmon_methods,
	.size    = sizeof(struct dummy_hwmon_softc),
};

DRIVER_MODULE(hwmon, dummy_hwmon, hwmon_driver, hwmon_devclass, 0, 0);
DRIVER_MODULE(dummy_hwmon, nexus, dummy_hwmon_driver, dummy_hwmon_devclass, 0, 0);

static void
dummy_hwmon_identify(driver_t *driver, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, "dummy_hwmon", 0);
}

static int
dummy_hwmon_probe(device_t dev)
{

	return 0;
}

static struct hwmon_ivars dummy_hwmon_ivars;

static int
dummy_hwmon_attach(device_t dev)
{
	device_t child;
	int error = 0;
	struct hwmon_ivars *hi = &dummy_hwmon_ivars;

	hi->hi_in_nchannels = 2;

	child = device_add_child(dev, "hwmon", -1);
	if (child == NULL) {
		error = ENXIO;
		goto out;
	}

	device_set_ivars(child, hi);

out:
	return error;
}

static int
dummy_hwmon_detach(device_t dev)
{
	int error = 0;

	return error;
}

static int
dummy_hwmon_in_get_label(device_t dev, int channel, const char **labelp)
{
	int error = 0;

	*labelp = "label";

	return error;
}

static int dummy_hwmon_min = 0;
static int
dummy_hwmon_in_get_min(device_t dev, int channel, int *valuep)
{

	*valuep = dummy_hwmon_min;
	return 0;
}

static int
dummy_hwmon_in_set_min(device_t dev, int channel, int value)
{

	dummy_hwmon_min = value;

	return 0;
}

static int dummy_hwmon_max = 0;
static int
dummy_hwmon_in_get_max(device_t dev, int channel, int *valuep)
{

	*valuep = dummy_hwmon_max;

	return 0;
}

static int
dummy_hwmon_in_set_max(device_t dev, int channel, int value)
{

	dummy_hwmon_max = value;

	return 0;
}

static int
dummy_hwmon_in_get_input(device_t dev, int channel, int *valuep)
{

	*valuep = 42;

	return 0;
}
#endif
