/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * YT8531C core: auto-negotiation, link status and PHY statistics.
 */

#include "yt8531c/yt8531c.h"

#include "yt8531c_internal.hpp"

namespace {

using namespace yt8531c::detail;

yt_u16 advertisement_from_modes(yt_u16 modes)
{
	yt_u16 value = YT8531C_ADV_SELECTOR_8023;

	if ((modes & YT8531C_MODE_10HD) != 0u)
		value |= YT8531C_ADV_10BASE_T_HALF;
	if ((modes & YT8531C_MODE_10FD) != 0u)
		value |= YT8531C_ADV_10BASE_T_FULL;
	if ((modes & YT8531C_MODE_100HD) != 0u)
		value |= YT8531C_ADV_100BASE_TX_HALF;
	if ((modes & YT8531C_MODE_100FD) != 0u)
		value |= YT8531C_ADV_100BASE_TX_FULL;
	if ((modes & YT8531C_MODE_PAUSE) != 0u)
		value |= YT8531C_ADV_PAUSE;
	if ((modes & YT8531C_MODE_ASYM_PAUSE) != 0u)
		value |= YT8531C_ADV_PAUSE_ASYM;

	return value;
}

yt_u16 ctrl1000_from_config(const yt8531c_config *cfg)
{
	yt_u16 modes = (cfg->advertise != 0u) ? cfg->advertise
					      : YT8531C_MODE_DEFAULT;
	yt_u16 value = 0u;

	if ((modes & YT8531C_MODE_1000FD) != 0u)
		value |= YT8531C_CTRL1000_ADV_1000_FULL;
	if ((modes & YT8531C_MODE_1000HD) != 0u)
		value |= YT8531C_CTRL1000_ADV_1000_HALF;

	switch (cfg->master_slave) {
	case YT8531C_MS_PREFER_MASTER:
		value |= YT8531C_CTRL1000_PREFER_MASTER;
		break;
	case YT8531C_MS_FORCE_MASTER:
		value |= YT8531C_CTRL1000_MASTER_SLAVE_EN |
			 YT8531C_CTRL1000_AS_MASTER;
		break;
	case YT8531C_MS_FORCE_SLAVE:
		value |= YT8531C_CTRL1000_MASTER_SLAVE_EN;
		break;
	case YT8531C_MS_PREFER_SLAVE:
	case YT8531C_MS_AUTO:
	default:
		break;
	}

	return value;
}

void apply_tx_clock_for_speed(yt8531c_dev *dev, yt_u16 speed)
{
	const yt8531c_config *cfg = &dev->cfg;
	yt_u8 inverted = 0u;

	if (cfg->tx_clk_adj_enable == 0u)
		return;

	switch (speed) {
	case YT8531C_SPEED_1000:
		inverted = cfg->tx_clk_invert_1000;
		break;
	case YT8531C_SPEED_100:
		inverted = cfg->tx_clk_invert_100;
		break;
	case YT8531C_SPEED_10:
		inverted = cfg->tx_clk_invert_10;
		break;
	default:
		return;
	}

	(void)ext_modify(dev, YT8531C_EXT_RGMII_CFG1,
			 YT8531C_RGMII1_TX_CLK_INVERT,
			 inverted != 0u ? YT8531C_RGMII1_TX_CLK_INVERT : 0u);
}

yt_u16 speed_from_status(yt_u16 spec_status)
{
	switch (spec_status & YT8531C_SPEC_STATUS_SPEED_MASK) {
	case YT8531C_SPEC_STATUS_SPEED_10:
		return YT8531C_SPEED_10;
	case YT8531C_SPEC_STATUS_SPEED_100:
		return YT8531C_SPEED_100;
	case YT8531C_SPEC_STATUS_SPEED_1000:
		return YT8531C_SPEED_1000;
	default:
		return YT8531C_SPEED_UNKNOWN;
	}
}

} /* namespace */

namespace yt8531c {
namespace detail {

yt_rc program_advertisement(yt8531c_dev *dev)
{
	yt_u16 modes = (dev->cfg.advertise != 0u) ? dev->cfg.advertise
						 : YT8531C_MODE_DEFAULT;
	yt_rc rc;

	rc = mii_write(dev, YT8531C_REG_ADVERTISE,
		       advertisement_from_modes(modes));
	if (rc != YT8531C_OK)
		return rc;

	/* Modify only the advertisement bits: test modes must survive. */
	return mii_modify(dev, YT8531C_REG_CTRL1000, YT8531C_CTRL1000_CFG_MASK,
			  ctrl1000_from_config(&dev->cfg));
}

} /* namespace detail */
} /* namespace yt8531c */

extern "C" {

yt_rc yt8531c_config_aneg(yt8531c_dev *dev)
{
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	rc = yt8531c::detail::program_advertisement(dev);
	if (rc != YT8531C_OK)
		return rc;

	/* Enable auto-negotiation, leave isolation, power down and loopback. */
	rc = yt8531c::detail::mii_modify(
		dev, YT8531C_REG_BMCR,
		static_cast<yt_u16>(YT8531C_BMCR_ANEG_ENABLE |
				    YT8531C_BMCR_RESTART_ANEG |
				    YT8531C_BMCR_ISOLATE |
				    YT8531C_BMCR_POWER_DOWN |
				    YT8531C_BMCR_LOOPBACK),
		static_cast<yt_u16>(YT8531C_BMCR_ANEG_ENABLE |
				    YT8531C_BMCR_RESTART_ANEG));
	if (rc != YT8531C_OK)
		return rc;

	dev->configured = 1u;
	return YT8531C_OK;
}

yt_rc yt8531c_aneg_done(yt8531c_dev *dev, yt_u8 *done)
{
	yt_u16 bmsr = 0u;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev) || done == nullptr)
		return YT8531C_ERR_ARG;

	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_BMSR, &bmsr);
	if (rc != YT8531C_OK)
		return rc;

	*done = ((bmsr & YT8531C_BMSR_ANEG_COMPLETE) != 0u) ? 1u : 0u;
	return YT8531C_OK;
}

yt_rc yt8531c_force_link(yt8531c_dev *dev, yt_u16 speed, yt_u8 duplex)
{
	yt_u16 set = YT8531C_BMCR_ANEG_ENABLE;
	yt_u16 mask = static_cast<yt_u16>(
		YT8531C_BMCR_ANEG_ENABLE | YT8531C_BMCR_SPEED_MASK |
		YT8531C_BMCR_DUPLEX | YT8531C_BMCR_ISOLATE |
		YT8531C_BMCR_POWER_DOWN | YT8531C_BMCR_RESTART_ANEG);

	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	if (speed == YT8531C_SPEED_UNKNOWN)
		return yt8531c_config_aneg(dev);

	if (duplex != YT8531C_DUPLEX_HALF && duplex != YT8531C_DUPLEX_FULL)
		return YT8531C_ERR_ARG;

	/* The chip implements 1000BASE-T full duplex only. */
	if (speed == YT8531C_SPEED_1000 && duplex == YT8531C_DUPLEX_HALF)
		return YT8531C_ERR_UNSUPPORTED;

	switch (speed) {
	case YT8531C_SPEED_10:
		break;
	case YT8531C_SPEED_100:
		set |= YT8531C_BMCR_SPEED_LSB;
		break;
	case YT8531C_SPEED_1000:
		set |= YT8531C_BMCR_SPEED_MSB;
		break;
	default:
		return YT8531C_ERR_ARG;
	}

	if (duplex == YT8531C_DUPLEX_FULL)
		set |= YT8531C_BMCR_DUPLEX;

	/* Clear auto-negotiation enable while forcing the link. */
	set = static_cast<yt_u16>(set & static_cast<yt_u16>(
					       ~YT8531C_BMCR_ANEG_ENABLE));

	return yt8531c::detail::mii_modify(dev, YT8531C_REG_BMCR, mask, set);
}

yt_rc yt8531c_read_status(yt8531c_dev *dev, yt8531c_status *status)
{
	yt_u16 bmsr = 0u;
	yt_u16 bmsr2 = 0u;
	yt_u16 spec = 0u;
	yt_u16 lpa = 0u;
	yt_u16 stat1000 = 0u;
	yt_u16 resolved;
	yt_u16 link;
	yt_u16 speed;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev) || status == nullptr)
		return YT8531C_ERR_ARG;

	yt8531c::detail::mem_zero(status, sizeof(*status));

	/* The link status bit latches low; read twice for the real-time value. */
	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_BMSR, &bmsr);
	if (rc != YT8531C_OK)
		return rc;
	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_BMSR, &bmsr2);
	if (rc != YT8531C_OK)
		return rc;

	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_SPEC_STATUS, &spec);
	if (rc != YT8531C_OK)
		return rc;

	link = ((spec & YT8531C_SPEC_STATUS_LINK) != 0u) ? 1u : 0u;
	resolved = ((spec & YT8531C_SPEC_STATUS_RESOLVED) != 0u) ? 1u : 0u;

	status->bmsr = bmsr2;
	status->spec_status = spec;
	status->link = link;
	status->autoneg_complete =
		((bmsr2 & YT8531C_BMSR_ANEG_COMPLETE) != 0u) ? 1u : 0u;
	status->resolved = resolved;
	status->mdix = 2u;
	status->master_slave = YT8531C_MS_UNKNOWN;

	if (link == 0u) {
		status->speed = YT8531C_SPEED_UNKNOWN;
		status->duplex = YT8531C_DUPLEX_UNKNOWN;
	} else {
		speed = speed_from_status(spec);
		status->speed = speed;
		status->duplex = ((spec & YT8531C_SPEC_STATUS_DUPLEX) != 0u)
					 ? YT8531C_DUPLEX_FULL
					 : YT8531C_DUPLEX_HALF;
		status->polarity_inverted =
			((spec & YT8531C_SPEC_STATUS_POLARITY) != 0u) ? 1u : 0u;
		status->jabber =
			((spec & YT8531C_SPEC_STATUS_JABBER) != 0u) ? 1u : 0u;
		status->speed_downgraded =
			((spec & YT8531C_SPEC_STATUS_DOWNGRADE) != 0u) ? 1u : 0u;

		if (status->autoneg_complete != 0u) {
			status->tx_pause =
				((spec & YT8531C_SPEC_STATUS_TX_PAUSE) != 0u)
					? 1u
					: 0u;
			status->rx_pause =
				((spec & YT8531C_SPEC_STATUS_RX_PAUSE) != 0u)
					? 1u
					: 0u;
		}

		if (resolved != 0u)
			status->mdix =
				((spec & YT8531C_SPEC_STATUS_MDIX) != 0u) ? 1u
									  : 0u;

		if (status->autoneg_complete != 0u) {
			rc = yt8531c::detail::mii_read(dev, YT8531C_REG_LPA,
						       &lpa);
			if (rc != YT8531C_OK)
				return rc;
			rc = yt8531c::detail::mii_read(dev,
						       YT8531C_REG_STAT1000,
						       &stat1000);
			if (rc != YT8531C_OK)
				return rc;

			status->lpa = lpa;
			status->stat1000 = stat1000;

			if (speed == YT8531C_SPEED_1000 &&
			    (stat1000 & YT8531C_STAT1000_MS_CONFIG_FAULT) ==
				    0u) {
				status->master_slave =
					((stat1000 &
					  YT8531C_STAT1000_MS_RESOLVED) != 0u)
						? YT8531C_MS_MASTER
						: YT8531C_MS_SLAVE;
			}
		}

		/* Per-speed TX clock inversion fixups (JH7110 style boards). */
		if (speed != dev->last_speed || dev->last_link == 0u)
			apply_tx_clock_for_speed(dev, speed);
	}

	status->link_changed = (link != dev->last_link) ? 1u : 0u;
	dev->last_link = static_cast<yt_u8>(link);
	dev->link = static_cast<yt_u8>(link);
	dev->speed = status->speed;
	dev->last_speed = status->speed;
	dev->duplex = static_cast<yt_u8>(status->duplex);

	return YT8531C_OK;
}

yt_rc yt8531c_get_features(yt8531c_dev *dev, yt8531c_features *features)
{
	yt_u16 bmsr = 0u;
	yt_u16 estatus = 0u;
	yt_u16 spec_func = 0u;
	yt_u16 eee_cap = 0u;
	yt_u16 modes = static_cast<yt_u16>(YT8531C_MODE_10HD |
					   YT8531C_MODE_10FD);
	yt_u16 flags = YT8531C_FEAT_TP | YT8531C_FEAT_WOL |
		       YT8531C_FEAT_LEDS | YT8531C_FEAT_SYNCE;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev) || features == nullptr)
		return YT8531C_ERR_ARG;

	yt8531c::detail::mem_zero(features, sizeof(*features));

	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_BMSR, &bmsr);
	if (rc != YT8531C_OK)
		return rc;

	if ((bmsr & YT8531C_BMSR_100BASE_X_HALF) != 0u)
		modes |= YT8531C_MODE_100HD;
	if ((bmsr & YT8531C_BMSR_100BASE_X_FULL) != 0u)
		modes |= YT8531C_MODE_100FD;
	if ((bmsr & YT8531C_BMSR_ANEG_ABILITY) != 0u)
		flags |= YT8531C_FEAT_AUTONEG;

	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_ESTATUS, &estatus);
	if (rc != YT8531C_OK)
		return rc;
	if ((estatus & YT8531C_ESTATUS_1000BASE_T_FULL) != 0u) {
		modes |= YT8531C_MODE_1000FD;
		flags |= YT8531C_FEAT_1000BASE_T;
	}
	if ((estatus & YT8531C_ESTATUS_1000BASE_T_HALF) != 0u)
		modes |= YT8531C_MODE_1000HD;

	modes |= YT8531C_MODE_PAUSE | YT8531C_MODE_ASYM_PAUSE;

	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_SPEC_FUNC, &spec_func);
	if (rc != YT8531C_OK)
		return rc;
	if ((spec_func & YT8531C_SPEC_FUNC_CROSS_MD_MASK) ==
	    YT8531C_SPEC_FUNC_CROSS_MD_AUTO)
		flags |= YT8531C_FEAT_MDIX_AUTO;

	if (yt8531c::detail::mmd_read(dev, YT8531C_MMD3, 0x0014u, &eee_cap) ==
	    YT8531C_OK) {
		if ((eee_cap & YT8531C_EEE_CAP_1000BASE_T) != 0u)
			flags |= YT8531C_FEAT_EEE_1000;
		if ((eee_cap & YT8531C_EEE_CAP_100BASE_TX) != 0u)
			flags |= YT8531C_FEAT_EEE_100;
	}

	features->modes = modes;
	features->flags = flags;
	return YT8531C_OK;
}

yt_rc yt8531c_get_counters(yt8531c_dev *dev, yt8531c_counters *counters)
{
	yt_u16 rx_errors = 0u;
	yt_u16 stat1000 = 0u;
	yt_u16 wake_errors = 0u;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev) || counters == nullptr)
		return YT8531C_ERR_ARG;

	yt8531c::detail::mem_zero(counters, sizeof(*counters));

	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_RX_ERR_COUNT,
				       &rx_errors);
	if (rc != YT8531C_OK)
		return rc;

	/* Reading the 1000BASE-T status register clears the idle error count. */
	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_STAT1000, &stat1000);
	if (rc != YT8531C_OK)
		return rc;

	rc = yt8531c::detail::mmd_read(dev, YT8531C_MMD3, 0x0016u,
				       &wake_errors);
	if (rc != YT8531C_OK)
		return rc;

	counters->rx_errors = rx_errors;
	counters->idle_errors =
		static_cast<yt_u16>(stat1000 & YT8531C_STAT1000_IDLE_ERR_MASK);
	counters->eee_wake_errors = wake_errors;
	return YT8531C_OK;
}

} /* extern "C" */
