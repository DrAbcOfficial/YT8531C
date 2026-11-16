/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * YT8531C portable PHY driver - public C API.
 *
 * The implementation is C++20 but exposes a plain C ABI and uses no C or C++
 * standard library facilities.  All states live in the caller-provided
 * yt8531c_dev object; no allocation is performed anywhere.
 *
 * Threading: see yt8531c_port.h.  Every function must be called with exclusive
 * access to the device context.
 */

#ifndef YT8531C_H
#define YT8531C_H

#include "yt8531c_config.h"
#include "yt8531c_port.h"
#include "yt8531c_regs.h"
#include "yt8531c_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Public device context.  Treat the members as read-only: they are exposed so
 * that callers can embed the object without dynamic allocation.  The reserved
 * area allows the implementation to grow without breaking the ABI.
 */
typedef struct yt8531c_dev {
	yt_u32 abi_version; /* YT8531C_VERSION */
	yt_u32 struct_size; /* sizeof(yt8531c_dev) */
	yt_u32 magic;	    /* YT8531C_DEV_MAGIC once initialised */
	yt_u32 chip_id;	    /* 0x4f51e91b for YT8531C */

	yt8531c_config cfg;
	yt8531c_port port;

	yt_u16 chip_config;
	yt_u16 int_mask;

	yt_u8 model;
	yt_u8 revision;
	yt_u8 ldo_voltage;
	yt_u8 probed;
	yt_u8 configured;
	yt_u8 link;
	yt_u16 speed;
	yt_u8 duplex;
	yt_u16 last_speed;
	yt_u8 last_link;
	yt_u8 wol_enabled;
	yt_u8 reserved[27];
} yt8531c_dev;

/* Version of the library, encoded as YT8531C_VERSION_Make(major, minor, patch). */
yt_u32 yt8531c_version(void);

/* sizeof(yt8531c_dev), for callers that cannot see the structure definition. */
yt_u32 yt8531c_context_size(void);

/* Fill @cfg with the documented chip defaults.  @cfg must not be NULL. */
void yt8531c_config_default(yt8531c_config *cfg);

/*
 * Initialise a device context.  The configuration and port are copied, so the
 * caller may release its own copies afterwards.  Returns YT8531C_ERR_ARG when
 * required callbacks are missing.  Does not touch the hardware.
 */
yt_rc yt8531c_init(yt8531c_dev *dev, const yt8531c_config *cfg,
		   const yt8531c_port *port);

/*
 * Read and validate the PHY identification registers, cache the strap
 * configuration and detect the RGMII I/O voltage.  Returns YT8531C_ERR_ID for
 * any device that is not a YT8531 family member and YT8531C_ERR_UNSUPPORTED
 * for known but unsupported siblings (for example YT8531S).
 */
yt_rc yt8531c_probe(yt8531c_dev *dev, yt8531c_id *id);

/*
 * Reset the PHY.  When the port provides a hardware reset callback, RESET_N is
 * toggled first; a software reset through BMCR is always issued afterwards and
 * polled until self-clearing.  The cached identification is refreshed.
 */
yt_rc yt8531c_reset(yt8531c_dev *dev);

/*
 * Apply the board configuration: RGMII delays and clock inversion, pad drive
 * strength, interrupt pin mode and mask, Sync-E clock output, auto-sleep and
 * PLL retention, jumbo frames, EEE advertisement, WOL and auto-negotiation
 * advertisement.  Safe to call repeatedly and after yt8531c_reset().
 *
 * Speed auto-downgrade settings are programmed here but, as documented by the
 * datasheet, only take effect after the next PHY reset.
 */
yt_rc yt8531c_config_init(yt8531c_dev *dev);

/* Update the advertisement and restart auto-negotiation, or force a link. */
yt_rc yt8531c_config_aneg(yt8531c_dev *dev);

/* Return 1 when auto-negotiation completed, 0 when still running. */
yt_rc yt8531c_aneg_done(yt8531c_dev *dev, yt_u8 *done);

/*
 * Force a fixed speed/duplex with auto-negotiation disabled, or restore
 * auto-negotiation with @speed == YT8531C_SPEED_UNKNOWN.  @duplex is ignored
 * when auto-negotiation is restored.
 */
yt_rc yt8531c_force_link(yt8531c_dev *dev, yt_u16 speed, yt_u8 duplex);

/* Read the current link status and decode speed, duplex, pause and MDIX. */
yt_rc yt8531c_read_status(yt8531c_dev *dev, yt8531c_status *status);

/* Report static capabilities plus the EEE abilities read from the chip. */
yt_rc yt8531c_get_features(yt8531c_dev *dev, yt8531c_features *features);

/* Power management: WOL-aware suspend and resume. */
yt_rc yt8531c_suspend(yt8531c_dev *dev);
yt_rc yt8531c_resume(yt8531c_dev *dev);

/* Interrupt mask and status handling (events are read-clear). */
yt_rc yt8531c_irq_set_mask(yt8531c_dev *dev, yt_u16 mask);
yt_rc yt8531c_irq_get_mask(yt8531c_dev *dev, yt_u16 *mask);
yt_rc yt8531c_irq_read(yt8531c_dev *dev, yt_u16 *events);
yt_rc yt8531c_irq_clear(yt8531c_dev *dev);
yt_rc yt8531c_irq_default_mask(yt_u16 *mask);

/* LED control; @index is 0..2. */
yt_rc yt8531c_led_set(yt8531c_dev *dev, yt_u8 index,
		      const yt8531c_led_cfg *cfg);
yt_rc yt8531c_led_get(yt8531c_dev *dev, yt_u8 index, yt8531c_led_cfg *cfg);
yt_rc yt8531c_led_blink_set(yt8531c_dev *dev, yt_u8 duty, yt_u8 freq1,
			    yt_u8 freq2);

/* Wake-on-LAN magic packet configuration. */
yt_rc yt8531c_wol_set(yt8531c_dev *dev, const yt_u8 mac[6], yt_u8 enable,
		      yt_u8 pmeb_mode, yt_u8 pulse_width);
yt_rc yt8531c_wol_get(yt8531c_dev *dev, yt_u8 mac[6], yt_u8 *enable,
		      yt_u8 *pmeb_mode, yt_u8 *pulse_width);

/* Energy Efficient Ethernet. */
yt_rc yt8531c_eee_get(yt8531c_dev *dev, yt8531c_eee *eee);
yt_rc yt8531c_eee_set(yt8531c_dev *dev, yt_u16 ability_mask);
/* Convert a target drive current in micro-amperes to a strength code. */
yt_rc yt8531c_drive_strength_from_current(yt_u8 ldo_voltage, yt_u32 microamps,
					  yt_u8 *strength);

/* Loopback, test mode and jumbo frame control. */
yt_rc yt8531c_set_loopback(yt8531c_dev *dev, yt_u8 mode);
yt_rc yt8531c_set_test_mode(yt8531c_dev *dev, yt_u8 mode);
yt_rc yt8531c_set_jumbo(yt8531c_dev *dev, yt_u8 enable);

/* Error counters (read-clear registers are cleared by reading). */
yt_rc yt8531c_get_counters(yt8531c_dev *dev, yt8531c_counters *counters);

/* Raw register access, useful for board bring-up and diagnostics. */
yt_rc yt8531c_mii_read(yt8531c_dev *dev, yt_u8 reg, yt_u16 *value);
yt_rc yt8531c_mii_write(yt8531c_dev *dev, yt_u8 reg, yt_u16 value);
yt_rc yt8531c_ext_read(yt8531c_dev *dev, yt_u16 reg, yt_u16 *value);
yt_rc yt8531c_ext_write(yt8531c_dev *dev, yt_u16 reg, yt_u16 value);
yt_rc yt8531c_mmd_read(yt8531c_dev *dev, yt_u8 devad, yt_u16 reg,
		       yt_u16 *value);
yt_rc yt8531c_mmd_write(yt8531c_dev *dev, yt_u8 devad, yt_u16 reg,
			yt_u16 value);

/* Snapshot of the most useful registers. */
yt_rc yt8531c_dump(yt8531c_dev *dev, yt8531c_snapshot *dump);

/* Human readable description of a return code; never returns NULL. */
const char *yt8531c_strerror(yt_rc rc);

#ifdef __cplusplus
}
#endif

#endif /* YT8531C_H */
