/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * YT8531C portable PHY driver - platform port interface.
 *
 * The core never calls into an operating system directly.  Every platform
 * provides:
 *   - clause 22 MDIO read/write callbacks for one PHY address,
 *   - a microsecond delay callback (used while polling reset completion),
 *   - optionally a hardware reset callback (RESET_N line),
 *   - optionally a logging callback.
 *
 * Concurrency: the core does not take any lock.  Each public entry point must
 * be serialised against all other entry points for the same device by the
 * caller (the Linux shim relies on the phylib device mutex plus the MDIO bus
 * lock).  Each individual MDIO transfer must also be atomic with respect to
 * other users of the bus; the callbacks provided by the Linux shim use the
 * phylib accessors that take the bus lock, which satisfies this requirement.
 */

#ifndef YT8531C_PORT_H
#define YT8531C_PORT_H

#include "yt8531c_types.h"

/* Optional capabilities advertised by the platform in yt8531c_port::caps. */
#define YT8531C_PORT_CAP_LOG 0x00000001u  /* log callback is valid */
#define YT8531C_PORT_CAP_RESET 0x00000002u /* hardware_reset is valid */

typedef struct yt8531c_port {
	/* Opaque pointer handed back to every callback. */
	void *ctx;

	/*
	 * Read/write one clause 22 register of the configured PHY address.
	 * Return 0 on success, a negative value on failure.  The implementation
	 * is responsible for truncating @reg to 5 bits and @phy_addr to 5 bits.
	 */
	int (*mdio_read)(void *ctx, yt_u8 phy_addr, yt_u8 reg, yt_u16 *value);
	int (*mdio_write)(void *ctx, yt_u8 phy_addr, yt_u8 reg, yt_u16 value);

	/*
	 * Sleep for at least @us microseconds.  Must not sleep in interrupt
	 * context; the driver only uses it from process context.
	 */
	void (*delay_us)(void *ctx, yt_u32 us);

	/*
	 * Optional diagnostics sink.  @msg is not NUL terminated and is only
	 * valid for the duration of the call.
	 */
	void (*log)(void *ctx, yt_u8 level, const char *msg, yt_u32 len);

	/*
	 * Optional hardware reset: assert RESET_N low for at least 10 ms,
	 * release it, then return.  The core waits reset_settle_us afterwards.
	 */
	int (*hardware_reset)(void *ctx);

	/* Bitmask of YT8531C_PORT_CAP_*. */
	yt_u32 caps;

	/* Reserved for forward-compatible extensions; must be zeroed. */
	yt_u32 reserved[4];
} yt8531c_port;

#define YT8531C_PORT_INITIALIZER                                    \
	{                                                            \
		.ctx = 0, .mdio_read = 0, .mdio_write = 0, .delay_us = 0, \
		.log = 0, .hardware_reset = 0, .caps = 0, .reserved = { 0 } \
	}

#endif /* YT8531C_PORT_H */
