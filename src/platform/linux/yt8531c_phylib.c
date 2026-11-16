/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * YT8531C Linux phylib driver.
 *
 * The portable core in src/core is compiled as C++20 without any runtime or
 * standard library dependency.  This file is the only C glue: it translates
 * phylib callbacks into core calls and maps device-tree properties to the
 * board configuration.
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/leds.h>
#include <linux/mii.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/property.h>

#include "yt8531c/yt8531c.h"

struct yt8531c_phy {
	struct yt8531c_dev dev;
	struct clk *clk;
};

static int yt8531c_errno(yt_rc rc)
{
	switch (rc) {
	case YT8531C_OK:
		return 0;
	case YT8531C_ERR_ARG:
		return -EINVAL;
	case YT8531C_ERR_IO:
		return -EIO;
	case YT8531C_ERR_TIMEOUT:
		return -ETIMEDOUT;
	case YT8531C_ERR_ID:
		return -ENODEV;
	case YT8531C_ERR_UNSUPPORTED:
		return -EOPNOTSUPP;
	case YT8531C_ERR_STATE:
		return -EINVAL;
	case YT8531C_ERR_RANGE:
		return -ERANGE;
	case YT8531C_ERR_VERIFY:
		return -EIO;
	default:
		return -EIO;
	}
}

static int yt8531c_port_read(void *ctx, yt_u8 phy_addr, yt_u8 reg,
			     yt_u16 *value)
{
	struct phy_device *phydev = ctx;
	int ret;

	(void)phy_addr;

	ret = phy_read(phydev, reg);
	if (ret < 0)
		return -1;

	*value = (yt_u16)ret;
	return 0;
}

static int yt8531c_port_write(void *ctx, yt_u8 phy_addr, yt_u8 reg,
			      yt_u16 value)
{
	struct phy_device *phydev = ctx;
	int ret;

	(void)phy_addr;

	ret = phy_write(phydev, reg, value);
	return (ret < 0) ? -1 : 0;
}

static void yt8531c_port_delay(void *ctx, yt_u32 us)
{
	(void)ctx;

	if (us > 2000u)
		fsleep(us);
	else
		udelay(us);
}

static void yt8531c_port_log(void *ctx, yt_u8 level, const char *msg,
			     yt_u32 len)
{
	struct phy_device *phydev = ctx;

	if (level >= YT8531C_LOG_ERROR)
		phydev_err(phydev, "%.*s\n", (int)len, msg);
	else if (level == YT8531C_LOG_WARN)
		phydev_warn(phydev, "%.*s\n", (int)len, msg);
	else
		phydev_dbg(phydev, "%.*s\n", (int)len, msg);
}

static yt8531c_port yt8531c_linux_port(struct phy_device *phydev)
{
	yt8531c_port port = YT8531C_PORT_INITIALIZER;

	port.ctx = phydev;
	port.mdio_read = yt8531c_port_read;
	port.mdio_write = yt8531c_port_write;
	port.delay_us = yt8531c_port_delay;
	port.log = yt8531c_port_log;
	port.caps = YT8531C_PORT_CAP_LOG;
	return port;
}

static int yt8531c_iface_to_core(phy_interface_t interface, yt_u8 *out)
{
	switch (interface) {
	case PHY_INTERFACE_MODE_RGMII:
		*out = YT8531C_IF_RGMII;
		return 0;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		*out = YT8531C_IF_RGMII_RXID;
		return 0;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		*out = YT8531C_IF_RGMII_TXID;
		return 0;
	case PHY_INTERFACE_MODE_RGMII_ID:
		*out = YT8531C_IF_RGMII_ID;
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

/*
 * Translate a device-tree delay property into a core delay.
 *
 * Delays below or equal to 2250 ps select the fine delay line only; values
 * between 1900 ps and 4150 ps additionally enable the 2 ns RXC delay, matching
 * the vendor reference driver's two-part table.
 */
static int yt8531c_parse_delay(struct phy_device *phydev, const char *name,
			       yt_u16 *delay_ps, yt_u8 *rxc_dly_en)
{
	struct device *dev = &phydev->mdio.dev;
	u32 value;

	if (rxc_dly_en != NULL)
		*rxc_dly_en = 0;

	if (device_property_read_u32(dev, name, &value) != 0) {
		*delay_ps = 1950;
		return 0;
	}

	if (value <= YT8531C_RGMII_DELAY_MAX_PS &&
	    (value % YT8531C_RGMII_DELAY_STEP_PS) == 0u) {
		*delay_ps = (yt_u16)value;
		return 0;
	}

	if (rxc_dly_en != NULL && value >= 1900u &&
	    value <= 1900u + YT8531C_RGMII_DELAY_MAX_PS &&
	    ((value - 1900u) % YT8531C_RGMII_DELAY_STEP_PS) == 0u) {
		*delay_ps = (yt_u16)(value - 1900u);
		*rxc_dly_en = 1;
		return 0;
	}

	phydev_warn(phydev, "invalid %s = %u, using 1950 ps\n", name, value);
	*delay_ps = 1950;
	return 0;
}

static int yt8531c_parse_clock_out(struct phy_device *phydev,
				   struct yt8531c_config *cfg)
{
	struct device *dev = &phydev->mdio.dev;
	u32 frequency;

	if (device_property_read_u32(dev, "motorcomm,clk-out-frequency-hz",
				     &frequency) != 0) {
		cfg->clk_out = YT8531C_CLK_OUT_OFF;
		return 0;
	}

	switch (frequency) {
	case 0:
		cfg->clk_out = YT8531C_CLK_OUT_OFF;
		return 0;
	case 25000000:
		cfg->clk_out = YT8531C_CLK_OUT_25M;
		return 0;
	case 125000000:
		cfg->clk_out = YT8531C_CLK_OUT_125M;
		return 0;
	default:
		phydev_err(phydev, "unsupported clk-out-frequency-hz %u\n",
			   frequency);
		return -EINVAL;
	}
}

static int yt8531c_build_config(struct phy_device *phydev,
				struct yt8531c_config *cfg)
{
	struct device *dev = &phydev->mdio.dev;
	int ret;

	yt8531c_config_default(cfg);

	cfg->phy_addr = phydev->mdio.addr;

	ret = yt8531c_iface_to_core(phydev->interface, &cfg->interface);
	if (ret != 0) {
		phydev_err(phydev, "unsupported interface %d\n",
			   phydev->interface);
		return ret;
	}

	yt8531c_parse_delay(phydev, "tx-internal-delay-ps", &cfg->tx_delay_ps,
			    NULL);
	yt8531c_parse_delay(phydev, "rx-internal-delay-ps", &cfg->rx_delay_ps,
			    &cfg->rx_clock_delay_en);

	cfg->auto_sleep_disabled =
		device_property_read_bool(dev, "motorcomm,auto-sleep-disabled");
	cfg->keep_pll_enabled =
		device_property_read_bool(dev, "motorcomm,keep-pll-enabled");
	cfg->tx_clk_adj_enable =
		device_property_read_bool(dev, "motorcomm,tx-clk-adj-enabled");
	cfg->tx_clk_invert_10 =
		device_property_read_bool(dev, "motorcomm,tx-clk-10-inverted");
	cfg->tx_clk_invert_100 =
		device_property_read_bool(dev, "motorcomm,tx-clk-100-inverted");
	cfg->tx_clk_invert_1000 =
		device_property_read_bool(dev, "motorcomm,tx-clk-1000-inverted");

	return yt8531c_parse_clock_out(phydev, cfg);
}

static int yt8531c_drive_strength(struct phy_device *phydev,
				  struct yt8531c_phy *priv, const char *name,
				  yt_u8 *strength)
{
	struct device *dev = &phydev->mdio.dev;
	u32 microamps;
	yt_u8 code;
	yt_rc rc;

	if (device_property_read_u32(dev, name, &microamps) != 0) {
		/* Reference driver default when the property is absent. */
		*strength = 3;
		return 0;
	}

	rc = yt8531c_drive_strength_from_current(priv->dev.ldo_voltage,
						 microamps, &code);
	if (rc != YT8531C_OK) {
		phydev_err(phydev, "%s = %u uA is not a valid setting\n", name,
			   microamps);
		return -EINVAL;
	}

	*strength = code;
	return 0;
}

static int yt8531c_resolve_drive_strengths(struct phy_device *phydev,
					   struct yt8531c_phy *priv)
{
	int ret;

	ret = yt8531c_drive_strength(phydev, priv,
				     "motorcomm,rx-clk-drv-microamp",
				     &priv->dev.cfg.rx_clk_drive_strength);
	if (ret != 0)
		return ret;

	ret = yt8531c_drive_strength(phydev, priv,
				     "motorcomm,rx-data-drv-microamp",
				     &priv->dev.cfg.rx_data_drive_strength);
	return ret;
}

static int yt8531c_phy_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct yt8531c_phy *priv;
	struct yt8531c_config cfg;
	yt8531c_port port;
	yt8531c_id id;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	priv->clk = devm_clk_get_optional_enabled(dev, NULL);
	if (IS_ERR(priv->clk))
		return dev_err_probe(dev, PTR_ERR(priv->clk),
				     "failed to enable the PHY clock\n");

	phydev->priv = priv;

	ret = yt8531c_build_config(phydev, &cfg);
	if (ret != 0)
		return ret;

	port = yt8531c_linux_port(phydev);
	ret = yt8531c_errno(yt8531c_init(&priv->dev, &cfg, &port));
	if (ret != 0)
		return ret;

	ret = yt8531c_errno(yt8531c_probe(&priv->dev, &id));
	if (ret != 0)
		return ret;

	return yt8531c_resolve_drive_strengths(phydev, priv);
}

static int yt8531c_phy_get_features(struct phy_device *phydev)
{
	struct yt8531c_phy *priv = phydev->priv;
	struct yt8531c_features features;
	int ret;

	if (priv == NULL)
		return -ENODEV;

	ret = yt8531c_errno(yt8531c_get_features(&priv->dev, &features));
	if (ret != 0)
		return ret;

	linkmode_zero(phydev->supported);

	if ((features.modes & YT8531C_MODE_10HD) != 0u)
		linkmode_set_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT,
				 phydev->supported);
	if ((features.modes & YT8531C_MODE_10FD) != 0u)
		linkmode_set_bit(ETHTOOL_LINK_MODE_10baseT_Full_BIT,
				 phydev->supported);
	if ((features.modes & YT8531C_MODE_100HD) != 0u)
		linkmode_set_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
				 phydev->supported);
	if ((features.modes & YT8531C_MODE_100FD) != 0u)
		linkmode_set_bit(ETHTOOL_LINK_MODE_100baseT_Full_BIT,
				 phydev->supported);
	if ((features.modes & YT8531C_MODE_1000FD) != 0u)
		linkmode_set_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
				 phydev->supported);
	if ((features.modes & YT8531C_MODE_1000HD) != 0u)
		linkmode_set_bit(ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
				 phydev->supported);
	if ((features.modes & YT8531C_MODE_PAUSE) != 0u)
		linkmode_set_bit(ETHTOOL_LINK_MODE_Pause_BIT, phydev->supported);
	if ((features.modes & YT8531C_MODE_ASYM_PAUSE) != 0u)
		linkmode_set_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT,
				 phydev->supported);
	if ((features.flags & YT8531C_FEAT_AUTONEG) != 0u)
		linkmode_set_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
				 phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_TP_BIT, phydev->supported);

	phydev->is_gigabit_capable = 1;
	return 0;
}

static int yt8531c_phy_config_init(struct phy_device *phydev)
{
	struct yt8531c_phy *priv = phydev->priv;

	if (priv == NULL)
		return -ENODEV;

	return yt8531c_errno(yt8531c_config_init(&priv->dev));
}

static int yt8531c_soft_reset(struct phy_device *phydev)
{
	struct yt8531c_phy *priv = phydev->priv;

	if (priv == NULL)
		return -ENODEV;

	return yt8531c_errno(yt8531c_reset(&priv->dev));
}

static u8 yt8531c_master_slave_from_ethtool(u8 cfg)
{
	switch (cfg) {
	case MASTER_SLAVE_CFG_MASTER_PREFERRED:
		return YT8531C_MS_PREFER_MASTER;
	case MASTER_SLAVE_CFG_SLAVE_PREFERRED:
		return YT8531C_MS_PREFER_SLAVE;
	case MASTER_SLAVE_CFG_MASTER_FORCE:
		return YT8531C_MS_FORCE_MASTER;
	case MASTER_SLAVE_CFG_SLAVE_FORCE:
		return YT8531C_MS_FORCE_SLAVE;
	case MASTER_SLAVE_CFG_UNKNOWN:
	case MASTER_SLAVE_CFG_UNSUPPORTED:
	default:
		return YT8531C_MS_AUTO;
	}
}

static int yt8531c_phy_config_aneg(struct phy_device *phydev)
{
	struct yt8531c_phy *priv = phydev->priv;
	yt_u16 modes = 0;

	if (priv == NULL)
		return -ENODEV;

	if (phydev->autoneg == AUTONEG_DISABLE)
		return yt8531c_errno(yt8531c_force_link(&priv->dev,
							phydev->speed,
							phydev->duplex));

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT,
			      phydev->advertising))
		modes |= YT8531C_MODE_10HD;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_10baseT_Full_BIT,
			      phydev->advertising))
		modes |= YT8531C_MODE_10FD;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
			      phydev->advertising))
		modes |= YT8531C_MODE_100HD;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_100baseT_Full_BIT,
			      phydev->advertising))
		modes |= YT8531C_MODE_100FD;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
			      phydev->advertising))
		modes |= YT8531C_MODE_1000FD;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT,
			      phydev->advertising))
		modes |= YT8531C_MODE_PAUSE;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT,
			      phydev->advertising))
		modes |= YT8531C_MODE_ASYM_PAUSE;

	priv->dev.cfg.advertise = (modes != 0u) ? modes : YT8531C_MODE_DEFAULT;
	priv->dev.cfg.master_slave =
		yt8531c_master_slave_from_ethtool(phydev->master_slave_set);

	return yt8531c_errno(yt8531c_config_aneg(&priv->dev));
}

static int yt8531c_phy_read_status(struct phy_device *phydev)
{
	struct yt8531c_phy *priv = phydev->priv;
	struct yt8531c_status status;
	int ret;

	if (priv == NULL)
		return -ENODEV;

	ret = yt8531c_errno(yt8531c_read_status(&priv->dev, &status));
	if (ret != 0)
		return ret;

	phydev->link = status.link;
	phydev->autoneg_complete = status.autoneg_complete;

	if (status.link == 0u) {
		phydev->speed = SPEED_UNKNOWN;
		phydev->duplex = DUPLEX_UNKNOWN;
		phydev->pause = 0;
		phydev->asym_pause = 0;
		phydev->mdix = ETH_TP_MDI_INVALID;
		linkmode_zero(phydev->lp_advertising);
		return 0;
	}

	switch (status.speed) {
	case YT8531C_SPEED_10:
		phydev->speed = SPEED_10;
		break;
	case YT8531C_SPEED_100:
		phydev->speed = SPEED_100;
		break;
	case YT8531C_SPEED_1000:
		phydev->speed = SPEED_1000;
		break;
	default:
		phydev->speed = SPEED_UNKNOWN;
		break;
	}

	phydev->duplex = (status.duplex == YT8531C_DUPLEX_FULL)
				 ? DUPLEX_FULL
				 : DUPLEX_HALF;
	phydev->pause = status.tx_pause;
	phydev->asym_pause = status.rx_pause;

	if (status.mdix == 2u)
		phydev->mdix = ETH_TP_MDI_INVALID;
	else if (status.mdix != 0u)
		phydev->mdix = ETH_TP_MDI_X;
	else
		phydev->mdix = ETH_TP_MDI;

	if (status.master_slave == YT8531C_MS_MASTER) {
		phydev->master_slave_get = MASTER_SLAVE_STATE_MASTER;
		phydev->master_slave_state = MASTER_SLAVE_STATE_MASTER;
	} else if (status.master_slave == YT8531C_MS_SLAVE) {
		phydev->master_slave_get = MASTER_SLAVE_STATE_SLAVE;
		phydev->master_slave_state = MASTER_SLAVE_STATE_SLAVE;
	} else {
		phydev->master_slave_get = MASTER_SLAVE_STATE_UNKNOWN;
		phydev->master_slave_state = MASTER_SLAVE_STATE_UNKNOWN;
	}

	linkmode_zero(phydev->lp_advertising);
	if (status.autoneg_complete != 0u) {
		mii_lpa_mod_linkmode_lpa_t(phydev->lp_advertising, status.lpa);
		mii_stat1000_mod_linkmode_lpa_t(phydev->lp_advertising,
						status.stat1000);
	}

	return 0;
}

static int yt8531c_phy_suspend(struct phy_device *phydev)
{
	struct yt8531c_phy *priv = phydev->priv;

	if (priv == NULL)
		return -ENODEV;

	return yt8531c_errno(yt8531c_suspend(&priv->dev));
}

static int yt8531c_phy_resume(struct phy_device *phydev)
{
	struct yt8531c_phy *priv = phydev->priv;

	if (priv == NULL)
		return -ENODEV;

	return yt8531c_errno(yt8531c_resume(&priv->dev));
}

static void yt8531c_phy_get_wol(struct phy_device *phydev,
			    struct ethtool_wolinfo *wol)
{
	struct yt8531c_phy *priv = phydev->priv;

	wol->supported = WAKE_MAGIC;
	wol->wolopts = 0;

	if (priv != NULL && priv->dev.wol_enabled != 0u)
		wol->wolopts = WAKE_MAGIC;
}

static int yt8531c_phy_set_wol(struct phy_device *phydev,
			      struct ethtool_wolinfo *wol)
{
	struct yt8531c_phy *priv = phydev->priv;
	yt_rc rc;

	if (priv == NULL)
		return -ENODEV;

	if ((wol->wolopts & WAKE_MAGIC) != 0u) {
		const u8 *address;

		if (phydev->attached_dev == NULL)
			return -ENODEV;

		address = phydev->attached_dev->dev_addr;
		if (!is_valid_ether_addr(address))
			return -EINVAL;

		rc = yt8531c_wol_set(&priv->dev, address, 1, 0,
				     YT8531C_WOL_PULSE_672MS);
	} else {
		rc = yt8531c_wol_set(&priv->dev, NULL, 0, 0, 0);
	}

	return yt8531c_errno(rc);
}

static int yt8531c_config_intr(struct phy_device *phydev)
{
	struct yt8531c_phy *priv = phydev->priv;
	yt_u16 mask;
	int ret;

	if (priv == NULL)
		return -ENODEV;

	mask = (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		       ? priv->dev.cfg.int_mask
		       : 0u;

	ret = yt8531c_errno(yt8531c_irq_clear(&priv->dev));
	if (ret != 0)
		return ret;

	return yt8531c_errno(yt8531c_irq_set_mask(&priv->dev, mask));
}

static irqreturn_t yt8531c_handle_interrupt(struct phy_device *phydev)
{
	struct yt8531c_phy *priv = phydev->priv;
	yt_u16 events = 0;

	if (priv == NULL)
		return IRQ_NONE;

	if (yt8531c_irq_read(&priv->dev, &events) != YT8531C_OK)
		return IRQ_NONE;

	return (events != 0u) ? IRQ_HANDLED : IRQ_NONE;
}

#define YT8531C_LED_RULES                                                \
	((1UL << TRIGGER_NETDEV_LINK) | (1UL << TRIGGER_NETDEV_LINK_10) | \
	 (1UL << TRIGGER_NETDEV_LINK_100) |                              \
	 (1UL << TRIGGER_NETDEV_LINK_1000) |                             \
	 (1UL << TRIGGER_NETDEV_FULL_DUPLEX) |                           \
	 (1UL << TRIGGER_NETDEV_HALF_DUPLEX) |                           \
	 (1UL << TRIGGER_NETDEV_TX) | (1UL << TRIGGER_NETDEV_RX))

static yt_u16 yt8531c_led_rules_to_triggers(unsigned long rules)
{
	yt_u16 triggers = 0;

	if (test_bit(TRIGGER_NETDEV_LINK, &rules))
		triggers |= YT8531C_LED_TRIG_LINK;
	if (test_bit(TRIGGER_NETDEV_LINK_10, &rules))
		triggers |= YT8531C_LED_TRIG_LINK_10;
	if (test_bit(TRIGGER_NETDEV_LINK_100, &rules))
		triggers |= YT8531C_LED_TRIG_LINK_100;
	if (test_bit(TRIGGER_NETDEV_LINK_1000, &rules))
		triggers |= YT8531C_LED_TRIG_LINK_1000;
	if (test_bit(TRIGGER_NETDEV_FULL_DUPLEX, &rules))
		triggers |= YT8531C_LED_TRIG_FULL_DUPLEX;
	if (test_bit(TRIGGER_NETDEV_HALF_DUPLEX, &rules))
		triggers |= YT8531C_LED_TRIG_HALF_DUPLEX;
	if (test_bit(TRIGGER_NETDEV_TX, &rules))
		triggers |= YT8531C_LED_TRIG_TX;
	if (test_bit(TRIGGER_NETDEV_RX, &rules))
		triggers |= YT8531C_LED_TRIG_RX;

	return triggers;
}

static void yt8531c_led_triggers_to_rules(yt_u16 triggers,
					  unsigned long *rules)
{
	if ((triggers & YT8531C_LED_TRIG_LINK_10) != 0u)
		__set_bit(TRIGGER_NETDEV_LINK_10, rules);
	if ((triggers & YT8531C_LED_TRIG_LINK_100) != 0u)
		__set_bit(TRIGGER_NETDEV_LINK_100, rules);
	if ((triggers & YT8531C_LED_TRIG_LINK_1000) != 0u)
		__set_bit(TRIGGER_NETDEV_LINK_1000, rules);
	if ((triggers & YT8531C_LED_TRIG_FULL_DUPLEX) != 0u)
		__set_bit(TRIGGER_NETDEV_FULL_DUPLEX, rules);
	if ((triggers & YT8531C_LED_TRIG_HALF_DUPLEX) != 0u)
		__set_bit(TRIGGER_NETDEV_HALF_DUPLEX, rules);
	if ((triggers & YT8531C_LED_TRIG_TX) != 0u)
		__set_bit(TRIGGER_NETDEV_TX, rules);
	if ((triggers & YT8531C_LED_TRIG_RX) != 0u)
		__set_bit(TRIGGER_NETDEV_RX, rules);
}

static int yt8531c_led_hw_is_supported(struct phy_device *phydev, u8 index,
				       unsigned long rules)
{
	if (index > 2u)
		return -EINVAL;

	if ((rules & ~YT8531C_LED_RULES) != 0UL)
		return -EOPNOTSUPP;

	return 0;
}

static int yt8531c_led_hw_control_set(struct phy_device *phydev, u8 index,
				      unsigned long rules)
{
	struct yt8531c_phy *priv = phydev->priv;
	struct yt8531c_led_cfg cfg = {};

	if (priv == NULL)
		return -ENODEV;

	cfg.triggers = yt8531c_led_rules_to_triggers(rules);
	cfg.force_mode = YT8531C_LED_FORCE_AUTO;
	cfg.blink_duty = YT8531C_AUTO;
	cfg.blink_freq1 = YT8531C_AUTO;
	cfg.blink_freq2 = YT8531C_AUTO;

	return yt8531c_errno(yt8531c_led_set(&priv->dev, index, &cfg));
}

static int yt8531c_led_hw_control_get(struct phy_device *phydev, u8 index,
				      unsigned long *rules)
{
	struct yt8531c_phy *priv = phydev->priv;
	struct yt8531c_led_cfg cfg;
	int ret;

	if (priv == NULL)
		return -ENODEV;

	ret = yt8531c_errno(yt8531c_led_get(&priv->dev, index, &cfg));
	if (ret != 0)
		return ret;

	yt8531c_led_triggers_to_rules(cfg.triggers, rules);
	return 0;
}

static struct phy_driver yt8531c_drivers[] = {
	{
		PHY_ID_MATCH_EXACT(YT8531C_PHY_ID_YT8531),
		.name = "Motorcomm YT8531C",
		.probe = yt8531c_phy_probe,
		.get_features = yt8531c_phy_get_features,
		.soft_reset = yt8531c_soft_reset,
		.config_init = yt8531c_phy_config_init,
		.config_aneg = yt8531c_phy_config_aneg,
		.read_status = yt8531c_phy_read_status,
		.suspend = yt8531c_phy_suspend,
		.resume = yt8531c_phy_resume,
		.set_wol = yt8531c_phy_set_wol,
		.get_wol = yt8531c_phy_get_wol,
		.config_intr = yt8531c_config_intr,
		.handle_interrupt = yt8531c_handle_interrupt,
		.led_hw_is_supported = yt8531c_led_hw_is_supported,
		.led_hw_control_set = yt8531c_led_hw_control_set,
		.led_hw_control_get = yt8531c_led_hw_control_get,
	},
};

module_phy_driver(yt8531c_drivers);

static const struct mdio_device_id yt8531c_phy_tbl[] = {
	{ PHY_ID_MATCH_EXACT(YT8531C_PHY_ID_YT8531) },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(mdio, yt8531c_phy_tbl);

MODULE_DESCRIPTION("Motorcomm YT8531C Gigabit Ethernet PHY driver");
MODULE_AUTHOR("YT8531C driver project");
MODULE_LICENSE("GPL");
