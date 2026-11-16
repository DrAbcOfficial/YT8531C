/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * YT8531C portable PHY driver - configuration, status and capability types.
 */

#ifndef YT8531C_CONFIG_H
#define YT8531C_CONFIG_H

#include "yt8531c_types.h"

/* RGMII interface variant, mirroring the four standard RGMII delay modes. */
typedef enum yt8531c_interface {
	YT8531C_IF_RGMII = 0,	 /* no internal delay */
	YT8531C_IF_RGMII_RXID = 1, /* PHY adds the RX clock delay */
	YT8531C_IF_RGMII_TXID = 2, /* PHY adds the TX clock delay */
	YT8531C_IF_RGMII_ID = 3,	 /* PHY adds both delays */
	YT8531C_IF_NONE = 0xff
} yt8531c_interface;

/* CLKOUT / Sync-E output configuration. */
typedef enum yt8531c_clk_out {
	YT8531C_CLK_OUT_OFF = 0,
	YT8531C_CLK_OUT_25M = 1,
	YT8531C_CLK_OUT_125M = 2
} yt8531c_clk_out;

typedef enum yt8531c_clk_src {
	YT8531C_CLK_SRC_AUTO = 0xff,
	YT8531C_CLK_SRC_PLL_125M = 0,
	YT8531C_CLK_SRC_UTP_RX = 1,
	YT8531C_CLK_SRC_SDS_RX = 2,
	YT8531C_CLK_SRC_DIGITAL = 3,
	YT8531C_CLK_SRC_REF_25M = 4,
	YT8531C_CLK_SRC_SSC_25M = 5
} yt8531c_clk_src;

/* 1000BASE-T master/slave preference. */
typedef enum yt8531c_master_slave {
	YT8531C_MS_AUTO = 0,
	YT8531C_MS_PREFER_MASTER = 1,
	YT8531C_MS_PREFER_SLAVE = 2,
	YT8531C_MS_FORCE_MASTER = 3,
	YT8531C_MS_FORCE_SLAVE = 4
} yt8531c_master_slave;

/* Resolved master/slave state reported by the status register. */
#define YT8531C_MS_UNKNOWN 0xffu
#define YT8531C_MS_SLAVE 0u
#define YT8531C_MS_MASTER 1u

/* Link mode bitmask used for advertisement and capabilities. */
#define YT8531C_MODE_10HD 0x0001u
#define YT8531C_MODE_10FD 0x0002u
#define YT8531C_MODE_100HD 0x0004u
#define YT8531C_MODE_100FD 0x0008u
#define YT8531C_MODE_1000HD 0x0010u
#define YT8531C_MODE_1000FD 0x0020u
#define YT8531C_MODE_SPEEDS 0x003fu
#define YT8531C_MODE_PAUSE 0x0100u
#define YT8531C_MODE_ASYM_PAUSE 0x0200u
/*
 * The chip only implements 1000BASE-T full duplex, so the default
 * advertisement deliberately excludes YT8531C_MODE_1000HD.
 */
#define YT8531C_MODE_DEFAULT                                             \
	(YT8531C_MODE_10HD | YT8531C_MODE_10FD | YT8531C_MODE_100HD |    \
	 YT8531C_MODE_100FD | YT8531C_MODE_1000FD | YT8531C_MODE_PAUSE | \
	 YT8531C_MODE_ASYM_PAUSE)

/* Capability flags reported by yt8531c_get_features(). */
#define YT8531C_FEAT_AUTONEG 0x0001u
#define YT8531C_FEAT_TP 0x0002u
#define YT8531C_FEAT_EEE_1000 0x0004u
#define YT8531C_FEAT_EEE_100 0x0008u
#define YT8531C_FEAT_1000BASE_T 0x0010u
#define YT8531C_FEAT_MDIX_AUTO 0x0020u
#define YT8531C_FEAT_WOL 0x0040u
#define YT8531C_FEAT_LEDS 0x0080u
#define YT8531C_FEAT_SYNCE 0x0100u

/* Speed values used by yt8531c_status and yt8531c_force_link(). */
#define YT8531C_SPEED_UNKNOWN 0u
#define YT8531C_SPEED_10 10u
#define YT8531C_SPEED_100 100u
#define YT8531C_SPEED_1000 1000u

#define YT8531C_DUPLEX_HALF 0u
#define YT8531C_DUPLEX_FULL 1u
#define YT8531C_DUPLEX_UNKNOWN 0xffu

/* LED trigger bitmask. */
#define YT8531C_LED_TRIG_LINK 0x0001u	  /* any speed */
#define YT8531C_LED_TRIG_LINK_10 0x0002u
#define YT8531C_LED_TRIG_LINK_100 0x0004u
#define YT8531C_LED_TRIG_LINK_1000 0x0008u
#define YT8531C_LED_TRIG_FULL_DUPLEX 0x0010u
#define YT8531C_LED_TRIG_HALF_DUPLEX 0x0020u
#define YT8531C_LED_TRIG_TX 0x0040u
#define YT8531C_LED_TRIG_RX 0x0080u
#define YT8531C_LED_TRIG_ACTIVITY 0x0100u /* either direction */
#define YT8531C_LED_TRIG_COLLISION 0x0200u
#define YT8531C_LED_TRIG_ALL 0x03ffu

/* LED force modes, matching LED_GENERAL_CFG force mode encoding. */
#define YT8531C_LED_FORCE_OFF 0u
#define YT8531C_LED_FORCE_ON 1u
#define YT8531C_LED_FORCE_BLINK2 2u
#define YT8531C_LED_FORCE_BLINK1 3u
#define YT8531C_LED_FORCE_AUTO 0xffu

/* LED blink frequencies. */
#define YT8531C_LED_BLINK_2HZ 0u
#define YT8531C_LED_BLINK_4HZ 1u
#define YT8531C_LED_BLINK_8HZ 2u
#define YT8531C_LED_BLINK_16HZ 3u

/* LED blink duty cycles. */
#define YT8531C_LED_DUTY_50 0u
#define YT8531C_LED_DUTY_67 1u
#define YT8531C_LED_DUTY_75 2u
#define YT8531C_LED_DUTY_83 3u
#define YT8531C_LED_DUTY_50_INV 4u
#define YT8531C_LED_DUTY_33 5u
#define YT8531C_LED_DUTY_25 6u
#define YT8531C_LED_DUTY_17 7u

/* Loopback and test modes. */
#define YT8531C_LOOPBACK_OFF 0u
#define YT8531C_LOOPBACK_DIGITAL 1u /* BMCR loopback */
#define YT8531C_LOOPBACK_REMOTE 2u  /* MDI receive to transmit */
#define YT8531C_LOOPBACK_REMOTE_UPLOAD 3u

#define YT8531C_TEST_MODE_NORMAL 0u
#define YT8531C_TEST_MODE_1 1u /* transmit waveform */
#define YT8531C_TEST_MODE_2 2u /* transmit jitter, master */
#define YT8531C_TEST_MODE_3 3u /* transmit jitter, slave */
#define YT8531C_TEST_MODE_4 4u /* transmit distortion */

/* Delay sentinels. */
#define YT8531C_DELAY_KEEP 0xffffu

/* LED configuration for one LED. */
typedef struct yt8531c_led_cfg {
	yt_u16 triggers;      /* YT8531C_LED_TRIG_* */
	yt_u8 force_mode;     /* YT8531C_LED_FORCE_* or YT8531C_AUTO */
	yt_u8 blink_duty;     /* YT8531C_LED_DUTY_* or YT8531C_AUTO */
	yt_u8 blink_freq1;    /* YT8531C_LED_BLINK_* or YT8531C_AUTO */
	yt_u8 blink_freq2;    /* YT8531C_LED_BLINK_* or YT8531C_AUTO */
	yt_u8 reserved[2];
} yt8531c_led_cfg;

/*
 * Board configuration.  Always start from yt8531c_config_default() and
 * override the fields that differ from the chip defaults; a zeroed structure
 * is not a valid configuration.
 */
typedef struct yt8531c_config {
	yt_u32 struct_size; /* set to sizeof(yt8531c_config) */
	yt_u16 advertise;   /* YT8531C_MODE_* */
	yt_u16 int_mask;    /* YT8531C_IRQ_* events to enable */
	yt_u16 tx_delay_ps; /* 0..2250, step 150, or YT8531C_DELAY_KEEP */
	yt_u16 rx_delay_ps;
	yt_u8 phy_addr; /* clause 22 PHY address, 0..31 */
	yt_u8 interface; /* yt8531c_interface */
	yt_u16 reset_timeout_ms; /* software reset poll timeout (default 500) */
	yt_u16 reset_settle_ms;  /* wait after hardware reset (default 20) */
	yt_u8 auto_sleep_disabled;
	yt_u8 keep_pll_enabled;
	yt_u8 jumbo_enable;
	yt_u8 eee_enable;
	yt_u8 master_slave; /* yt8531c_master_slave */
	yt_u8 rx_clock_delay_en; /* 0/1/YT8531C_AUTO */
	yt_u8 tx_clock_invert;
	yt_u8 tx_clk_adj_enable;
	yt_u8 tx_clk_invert_10;
	yt_u8 tx_clk_invert_100;
	yt_u8 tx_clk_invert_1000;
	yt_u8 rx_clk_drive_strength; /* 0..7 or YT8531C_AUTO */
	yt_u8 rx_data_drive_strength;
	yt_u8 mdio_drive_strength;
	yt_u8 led_drive_strength;
	yt_u8 synce_drive_strength;
	yt_u8 int_od_enable;   /* 0/1/YT8531C_AUTO */
	yt_u8 int_active_high; /* 0/1/YT8531C_AUTO */
	yt_u8 clk_out;	       /* yt8531c_clk_out */
	yt_u8 clk_out_src;     /* yt8531c_clk_src */
	yt_u8 clk_out_during_link_down;
	yt_u8 wol_enable;
	yt_u8 wol_pmeb_mode;
	yt_u8 wol_pulse_width; /* YT8531C_WOL_PULSE_* */
	yt_u8 speed_downgrade_enable; /* 0/1/YT8531C_AUTO */
	yt_u8 speed_downgrade_retry;  /* 0..7/YT8531C_AUTO */
	yt_u8 phyaddr0_respond;	      /* 0/1/YT8531C_AUTO */
	yt_u8 broadcast_enable;
	yt_u8 broadcast_addr;
	yt_u8 mac[6]; /* station address used for WOL */
	yt_u8 reserved[16];
} yt8531c_config;

/* Result of yt8531c_probe(). */
typedef struct yt8531c_id {
	yt_u32 id;	/* 0x4f51e91b for YT8531C */
	yt_u16 id1;	/* register 0x02 */
	yt_u16 id2;	/* register 0x03 */
	yt_u16 chip_config; /* extended register 0xa001 */
	yt_u8 model;	/* bits 9:4 of ID2 */
	yt_u8 revision; /* bits 3:0 of ID2 */
	yt_u8 ldo_voltage; /* YT8531C_LDO_VOLTAGE_* */
	yt_u8 supported;   /* 1 when the core drives this device */
} yt8531c_id;

/* Result of yt8531c_read_status(). */
typedef struct yt8531c_status {
	yt_u16 bmsr;
	yt_u16 lpa;
	yt_u16 stat1000;
	yt_u16 spec_status;
	yt_u16 link;	/* current link state, 1 = up */
	yt_u16 link_changed;
	yt_u16 speed;	/* YT8531C_SPEED_* */
	yt_u16 duplex;	/* YT8531C_DUPLEX_* */
	yt_u16 autoneg_complete;
	yt_u16 resolved;
	yt_u16 mdix; /* 0 = MDI, 1 = MDIX, 2 = unknown */
	yt_u16 polarity_inverted;
	yt_u16 jabber;
	yt_u16 speed_downgraded;
	yt_u16 tx_pause;
	yt_u16 rx_pause;
	yt_u16 master_slave; /* YT8531C_MS_* */
	yt_u16 reserved[6];
} yt8531c_status;

/* Result of yt8531c_get_features(). */
typedef struct yt8531c_features {
	yt_u16 modes; /* YT8531C_MODE_* */
	yt_u16 flags; /* YT8531C_FEAT_* */
	yt_u16 reserved[4];
} yt8531c_features;

/* EEE state and capabilities. */
typedef struct yt8531c_eee {
	yt_u16 capabilities;	/* MMD3 0x0014 */
	yt_u16 local_ability;	/* MMD7 0x003c */
	yt_u16 lp_ability;	/* MMD7 0x003d */
	yt_u16 pcs_status;	/* MMD3 0x0001 */
	yt_u16 wake_errors;	/* MMD3 0x0016 */
} yt8531c_eee;

/* Error counters exposed by the chip. */
typedef struct yt8531c_counters {
	yt_u16 rx_errors;    /* register 0x15 */
	yt_u16 idle_errors;  /* register 0x0a bits 7:0 */
	yt_u16 eee_wake_errors;
} yt8531c_counters;

/* Raw register snapshot for diagnostics. */
typedef struct yt8531c_ext_reg {
	yt_u16 addr;
	yt_u16 value;
} yt8531c_ext_reg;

typedef struct yt8531c_mmd_reg {
	yt_u8 devad;
	yt_u8 reg;
	yt_u16 value;
} yt8531c_mmd_reg;

#define YT8531C_DUMP_MII_COUNT 32
#define YT8531C_DUMP_EXT_COUNT 20
#define YT8531C_DUMP_MMD_COUNT 8

typedef struct yt8531c_snapshot {
	yt_u16 mii[YT8531C_DUMP_MII_COUNT]; /* 0x00..0x1f */
	yt8531c_ext_reg ext[YT8531C_DUMP_EXT_COUNT];
	yt8531c_mmd_reg mmd[YT8531C_DUMP_MMD_COUNT];
	yt_u8 ext_count;
	yt_u8 mmd_count;
	yt_u8 reserved[2];
} yt8531c_snapshot;

#endif /* YT8531C_CONFIG_H */
