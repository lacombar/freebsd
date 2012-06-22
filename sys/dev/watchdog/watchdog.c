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
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/kdb.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/sbuf.h>
#include <sys/watchdog.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <dev/watchdog/watchdogvar.h>

#include "watchdog_if.h"

struct watchdog_softc
{
	char *		ws_capabilities_actions;
	char *		ws_capabilities_max_timeout;
	struct timespec		ws_timeout;
	watchdog_action_t	ws_action;
	unsigned int		ws_flags;
#define WATCHDOG_RUNNING	0x01
#define WATCHDOG_IMMUTABLE	0x02
#define WATCHDOG_FLAGS_DESC	\
	"\20\1running\2immutable"
};

/*
 * Prototypes
 */
/* Sysctl helper */
static int watchdog_register_sysctl(device_t);

/* Sysctl hooks */
static int watchdog_sysctl_config_timeout(SYSCTL_HANDLER_ARGS);
static int watchdog_sysctl_config_action(SYSCTL_HANDLER_ARGS);
static int watchdog_sysctl_enable(SYSCTL_HANDLER_ARGS);
static int watchdog_sysctl_disable(SYSCTL_HANDLER_ARGS);
static int watchdog_sysctl_rearm(SYSCTL_HANDLER_ARGS);
static int watchdog_sysctl_immutable(SYSCTL_HANDLER_ARGS);
static int watchdog_sysctl_state(SYSCTL_HANDLER_ARGS);

/* device(4) interface */
static device_probe_t watchdog_probe;
static device_attach_t watchdog_attach;
static device_detach_t watchdog_detach;

/* watchdog(4) interface */
static watchdog_enable_t watchdog_enable;
static watchdog_disable_t watchdog_disable;
static watchdog_configure_t watchdog_configure;
static watchdog_rearm_t watchdog_rearm;

/*
 * Local data
 */
static device_method_t watchdog_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_probe,		watchdog_probe),
	DEVMETHOD(device_attach,	watchdog_attach),
	DEVMETHOD(device_detach,	watchdog_detach),

	DEVMETHOD_END
};

/*
 * Public data
 */
devclass_t watchdog_devclass;

driver_t watchdog_driver =
{
	.name    = "watchdog",
	.methods = watchdog_methods,
	.size    = sizeof(struct watchdog_softc),
};

/*
 * Helpers inlines
 */
static __inline void
watchdog_config_init(device_t dev)
{
	struct watchdog_softc *ws = device_get_softc(dev);

	ws->ws_timeout.tv_sec = 0;
	ws->ws_timeout.tv_nsec = 0;

	ws->ws_action = 0;
}

static __inline int
watchdog_config_is_valid(device_t dev)
{
	struct watchdog_softc *ws = device_get_softc(dev);

	return (ws->ws_timeout.tv_sec > 0 && ws->ws_action != 0);
}

static __inline uint32_t
string2bit(char *buf, const char *bitmask_desc)
{
	const char *s;
	uint32_t ret = 0;

	s = strstr(bitmask_desc, buf);
	if (s == NULL)
		goto out;

	ret = 1 << (*(s - 1) - 1);

out:
	return ret;
}

/*
 * Function definitions
 */
/* Sysctl helper */
static int
watchdog_register_sysctl(device_t dev)
{
	struct sysctl_oid_list *child, *config_list, *capabilities_list;
	struct sysctl_oid *tree, *config_node, *capabilities_node;;
	struct sysctl_ctx_list *ctx;
	struct watchdog_softc *ws = device_get_softc(dev);

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	capabilities_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "capabilities",
	    CTLFLAG_RD, NULL, "Capabilities");
	capabilities_list = SYSCTL_CHILDREN(capabilities_node);


	SYSCTL_ADD_STRING(ctx, capabilities_list, OID_AUTO, "actions",
	    CTLFLAG_RD, ws->ws_capabilities_actions, 0,
	    "Action triggered upon timeout");

	SYSCTL_ADD_STRING(ctx, capabilities_list, OID_AUTO, "min_timeout",
	    CTLFLAG_RD, "1", 0,
	    "Minimum timeout after which <action> is triggered");

	SYSCTL_ADD_STRING(ctx, capabilities_list, OID_AUTO, "max_timeout",
	    CTLFLAG_RD, ws->ws_capabilities_max_timeout, 0,
	    "Minimum timeout after which <action> is triggered");

	config_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "config",
	    CTLFLAG_RD, NULL, "Configuration");
	config_list = SYSCTL_CHILDREN(config_node);

	SYSCTL_ADD_PROC(ctx, config_list, OID_AUTO, "timeout",
	    CTLTYPE_STRING | CTLFLAG_RW, dev, 0,
	    watchdog_sysctl_config_timeout, "A",
	    "Configured timeout");

	SYSCTL_ADD_PROC(ctx, config_list, OID_AUTO, "action",
	    CTLTYPE_STRING | CTLFLAG_RW, dev, 0,
	    watchdog_sysctl_config_action, "A",
	    "Configured action");

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "enable",
	    CTLTYPE_UINT | CTLFLAG_RW, dev, 0,
	    watchdog_sysctl_enable, "IU",
	    "Enable the watchdog");

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "disable",
	    CTLTYPE_UINT | CTLFLAG_RW, dev, 0,
	    watchdog_sysctl_disable, "IU",
	    "Disable the watchdog");

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rearm",
	    CTLTYPE_UINT | CTLFLAG_RW, dev, 0,
	    watchdog_sysctl_rearm, "IU",
	    "Re-arm the watchdog");

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "immutable",
	    CTLTYPE_UINT | CTLFLAG_RW, dev, 0,
	    watchdog_sysctl_immutable, "IU",
	    "Forbid the watchdog to be disabled, could not be unset");

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "state",
	    CTLTYPE_UINT | CTLFLAG_RD, dev, 0,
	    watchdog_sysctl_state, "A",
	    "");

	return 0;
}

/* Sysctl hooks */
static int
watchdog_sysctl_config_timeout(SYSCTL_HANDLER_ARGS)
{
	device_t dev = oidp->oid_arg1;
	struct watchdog_softc *ws = device_get_softc(dev);
	struct sbuf *sb;
	int error = 0;

	if (req->newptr != NULL) {
		struct timespec ts;

		char *endp;
		int new;

		new = strtol(req->newptr, &endp, 0);
		if (*endp != '\0' || new <= 0) {
			error = EINVAL;
			goto out;
		}

		watchdog_get_max_timeout(dev, &ts);
		if (new > ts.tv_sec) {
			error = ENOTSUP;
			goto out;
		}

		if ((ws->ws_flags & WATCHDOG_IMMUTABLE) != 0) {
			error = EPERM;
			goto out;
		}

		ws->ws_timeout.tv_sec = new;
		ws->ws_timeout.tv_nsec = 0;
	}

	sb = sbuf_new_auto();
	sbuf_printf(sb, "%d", ws->ws_timeout.tv_sec);
	sbuf_finish(sb);

	error = sysctl_handle_string(oidp, sbuf_data(sb), sbuf_len(sb), req);

	sbuf_delete(sb);

out:
	return error;
}

static int
watchdog_sysctl_config_action(SYSCTL_HANDLER_ARGS)
{
	device_t dev = oidp->oid_arg1;
	struct watchdog_softc *ws = device_get_softc(dev);
	struct sbuf *sb;
	int error = 0;

	if (req->newptr != NULL) {
		struct watchdog_ivars *wi = device_get_ivars(dev);
		watchdog_action_t action;

		action = string2bit(req->newptr, WATCHDOG_ACTION_DESC);
		if (action == 0) {
			error = EINVAL;
			goto out;
		}

		if ((wi->wi_actions & action) == 0) {
			error = ENOTSUP;
			goto out;
		}

		if ((ws->ws_flags & WATCHDOG_IMMUTABLE) != 0) {
			error = EPERM;
			goto out;
		}

		ws->ws_action = action;
	}

	sb = sbuf_new_auto();

	if (ws->ws_action != 0)
		sbuf_printf(sb, "%b", ws->ws_action, WATCHDOG_ACTION_DESC);
	else
		sbuf_printf(sb, "<none>");
	sbuf_finish(sb);

	error = sysctl_handle_string(oidp, sbuf_data(sb), sbuf_len(sb), req);

	sbuf_delete(sb);

out:
	return error;
}

static int
watchdog_sysctl_enable(SYSCTL_HANDLER_ARGS)
{
	device_t dev = oidp->oid_arg1;
	struct watchdog_softc *ws = device_get_softc(dev);
	int error = 0;

	if (req->newptr != NULL) {
		int new = *(int *)req->newptr;

		if (new != 1) {
			error = EINVAL;
			goto out;
		}

		if ((ws->ws_flags & WATCHDOG_RUNNING) != 0) {
			error = EINVAL;
			goto out;
		}

		error = watchdog_configure(dev, &ws->ws_timeout, ws->ws_action);
		if (error != 0)
			goto out;

		error = watchdog_enable(dev);
		if (error != 0)
			goto out;

		ws->ws_flags |= WATCHDOG_RUNNING;
	}

out:
	return error;
}

static int
watchdog_sysctl_disable(SYSCTL_HANDLER_ARGS)
{
	device_t dev = oidp->oid_arg1;
	struct watchdog_softc *ws = device_get_softc(dev);
	int error = 0;

	if (req->newptr != NULL) {
		int new = *(int *)req->newptr;

		if (new != 1) {
			error = EINVAL;
			goto out;
		}

		if ((ws->ws_flags & WATCHDOG_RUNNING) == 0) {
			error = EINVAL;
			goto out;
		}

		if ((ws->ws_flags & WATCHDOG_IMMUTABLE) != 0) {
			error = EPERM;
			goto out;
		}

		error = watchdog_disable(dev);
		if (error == 0)
			ws->ws_flags &= ~WATCHDOG_RUNNING;
	}

out:
	return error;
}

static int
watchdog_sysctl_rearm(SYSCTL_HANDLER_ARGS)
{
	device_t dev = oidp->oid_arg1;
	struct watchdog_softc *ws = device_get_softc(dev);
	int error = 0;

	if (req->newptr != NULL) {
		int new = *(int *)req->newptr;

		if (new != 1) {
			error = EINVAL;
			goto out;
		}

		if ((ws->ws_flags & WATCHDOG_RUNNING) == 0) {
			error = EINVAL;
			goto out;
		}

		error = watchdog_rearm(dev);
	}

out:
	return error;
}

static int
watchdog_sysctl_immutable(SYSCTL_HANDLER_ARGS)
{
	device_t dev = oidp->oid_arg1;
	struct watchdog_softc *ws = device_get_softc(dev);
	int ret, error = 0;

	if (req->newptr != NULL) {
		int new = *(int *)req->newptr;

		if (new > 1 || new < 0) {
			error = EINVAL;
			goto out;
		}

		if ((ws->ws_flags & WATCHDOG_IMMUTABLE) != 0 && new == 0) {
			error = EPERM;
			goto out;
		}

		ws->ws_flags |= WATCHDOG_IMMUTABLE;
	}

	ret = ((ws->ws_flags & WATCHDOG_IMMUTABLE) != 0) ? 1 : 0;
	error = sysctl_handle_int(oidp, &ret, 0, req);
out:
	return error;
}

static int
watchdog_sysctl_state(SYSCTL_HANDLER_ARGS)
{
	device_t dev = oidp->oid_arg1;
	struct watchdog_softc *ws = device_get_softc(dev);
	struct sbuf *sb;
	int error;

	sb = sbuf_new_auto();
	sbuf_printf(sb, "%b", ws->ws_flags, WATCHDOG_FLAGS_DESC);
	sbuf_finish(sb);

	error = sysctl_handle_string(oidp, sbuf_data(sb), sbuf_len(sb), req);

	sbuf_delete(sb);

	return error;
}

/* device(4) interface */
static int
watchdog_probe(device_t dev)
{

	return 0;
}

static int
watchdog_attach(device_t dev)
{
	struct watchdog_softc *ws = device_get_softc(dev);
	struct watchdog_ivars *wi = device_get_ivars(dev);
	struct timespec ts;
	struct sbuf *sb;
	int error = 0;

	if (wi == NULL) {
		error = EINVAL;
		goto out;
	}

	/* XXX al:
	 * Grrrrr.... it would *really* be nice if SYSCTL_ADD_STRING() allowed
	 * temporary stored string. We would not have to cache these...
	 */
	sb = sbuf_new_auto();
	sbuf_printf(sb, "%b", wi->wi_actions, WATCHDOG_ACTION_DESC);
	sbuf_finish(sb);

	ws->ws_capabilities_actions = strdup(sbuf_data(sb), M_DEVBUF);

	watchdog_get_max_timeout(dev, &ts);

	sbuf_clear(sb);
	sbuf_printf(sb, "%d", ts.tv_sec);
	sbuf_finish(sb);
	ws->ws_capabilities_max_timeout = strdup(sbuf_data(sb), M_DEVBUF);

	sbuf_delete(sb);

	error = watchdog_register_sysctl(dev);
	if (error != 0)
		goto out;

	/* */
	watchdog_config_init(dev);

out:
	return error;
}

static int
watchdog_detach(device_t dev)
{
	struct watchdog_softc *ws = device_get_softc(dev);

	free(ws->ws_capabilities_max_timeout, M_DEVBUF);
	free(ws->ws_capabilities_actions, M_DEVBUF);

	return 0;
}

/* watchdog(4) interface */
static int
watchdog_enable(device_t dev)
{
	device_t parent = device_get_parent(dev);

	return WATCHDOG_ENABLE(parent);
}

static int
watchdog_disable(device_t dev)
{
	device_t parent = device_get_parent(dev);

	return WATCHDOG_DISABLE(parent);
}

static int
watchdog_configure(device_t dev, struct timespec *ts, watchdog_action_t action)
{
	device_t parent = device_get_parent(dev);

	return WATCHDOG_CONFIGURE(parent, ts, action);
}

static int
watchdog_rearm(device_t dev)
{
	device_t parent = device_get_parent(dev);

	return WATCHDOG_REARM(parent);
}
