/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * YT8531C core: device initialisation, reset, board configuration and power
 * management.
 */

#include "yt8531c/yt8531c.h"

#include "yt8531c_internal.hpp"

namespace {

using namespace yt8531c::detail;

yt_rc validate_config(const yt8531c_config *cfg)
{
	/* Reject configurations built against a different structure layout. */
	if (cfg->struct_size != static_cast<yt_u32>(sizeof(*cfg)))
		return YT8531C_ERR_ARG;

	if ((cfg->int_mask & static_cast<yt_u16>(~YT8531C_IRQ_MASK_ALL)) != 0u)
		return YT8531C_ERR_ARG;

	if (cfg->phy_addr > 31u)
		return YT8531C_ERR_ARG;

	if (cfg->interface > static_cast<yt_u8>(YT8531C_IF_RGMII_ID) &&
	    cfg->interface != static_cast<yt_u8>(YT8531C_IF_NONE))
		return YT8531C_ERR_ARG;

	if (cfg->tx_delay_ps != YT8531C_DELAY_KEEP &&
	    (cfg->tx_delay_ps > YT8531C_RGMII_DELAY_MAX_PS ||
	     (cfg->tx_delay_ps % YT8531C_RGMII_DELAY_STEP_PS) != 0u))
		return YT8531C_ERR_RANGE;

	if (cfg->rx_delay_ps != YT8531C_DELAY_KEEP &&
	    (cfg->rx_delay_ps > YT8531C_RGMII_DELAY_MAX_PS ||
	     (cfg->rx_delay_ps % YT8531C_RGMII_DELAY_STEP_PS) != 0u))
		return YT8531C_ERR_RANGE;

	if (cfg->rx_clock_delay_en != YT8531C_AUTO && cfg->rx_clock_delay_en > 1u)
		return YT8531C_ERR_ARG;
	if (cfg->tx_clock_invert > 1u || cfg->tx_clk_adj_enable > 1u ||
	    cfg->tx_clk_invert_10 > 1u || cfg->tx_clk_invert_100 > 1u ||
	    cfg->tx_clk_invert_1000 > 1u)
		return YT8531C_ERR_ARG;

	if ((cfg->rx_clk_drive_strength != YT8531C_AUTO &&
	     cfg->rx_clk_drive_strength > 7u) ||
	    (cfg->rx_data_drive_strength != YT8531C_AUTO &&
	     cfg->rx_data_drive_strength > 7u))
		return YT8531C_ERR_ARG;

	if ((cfg->mdio_drive_strength != YT8531C_AUTO &&
	     cfg->mdio_drive_strength > 3u) ||
	    (cfg->led_drive_strength != YT8531C_AUTO &&
	     cfg->led_drive_strength > 3u) ||
	    (cfg->synce_drive_strength != YT8531C_AUTO &&
	     cfg->synce_drive_strength > 3u))
		return YT8531C_ERR_ARG;

	if ((cfg->int_od_enable != YT8531C_AUTO && cfg->int_od_enable > 1u) ||
	    (cfg->int_active_high != YT8531C_AUTO &&
	     cfg->int_active_high > 1u))
		return YT8531C_ERR_ARG;

	if (cfg->clk_out > static_cast<yt_u8>(YT8531C_CLK_OUT_125M))
		return YT8531C_ERR_ARG;
	if (cfg->clk_out_src != static_cast<yt_u8>(YT8531C_CLK_SRC_AUTO) &&
	    cfg->clk_out_src > static_cast<yt_u8>(YT8531C_CLK_SRC_SSC_25M))
		return YT8531C_ERR_ARG;
	if (cfg->clk_out_during_link_down > 1u)
		return YT8531C_ERR_ARG;

	if (cfg->master_slave > static_cast<yt_u8>(YT8531C_MS_FORCE_SLAVE))
		return YT8531C_ERR_ARG;

	if (cfg->wol_pulse_width > YT8531C_WOL_PULSE_672MS)
		return YT8531C_ERR_ARG;

	if (cfg->broadcast_addr > 31u)
		return YT8531C_ERR_ARG;

	if (cfg->speed_downgrade_enable != YT8531C_AUTO &&
	    cfg->speed_downgrade_enable > 1u)
		return YT8531C_ERR_ARG;
	if (cfg->speed_downgrade_retry != YT8531C_AUTO &&
	    cfg->speed_downgrade_retry > 7u)
		return YT8531C_ERR_ARG;

	if (cfg->phyaddr0_respond != YT8531C_AUTO && cfg->phyaddr0_respond > 1u)
		return YT8531C_ERR_ARG;

	return YT8531C_OK;
}

yt_rc apply_rgmii(yt8531c_dev *dev)
{
	const yt8531c_config *cfg = &dev->cfg;
	yt_u16 tx_steps = 0u;
	yt_u16 rx_steps = 0u;
	yt_u16 tx_mask = YT8531C_RGMII1_TX_DELAY_MASK;
	yt_u16 rx_mask = YT8531C_RGMII1_RX_DELAY_MASK;
	yt_u16 rxc_mask = YT8531C_CHIP_CFG_RXC_DLY_EN;
	yt_u16 rxc_set = 0u;
	yt_u16 value = 0u;
	yt_rc rc;

	if (cfg->interface == static_cast<yt_u8>(YT8531C_IF_NONE))
		return YT8531C_ERR_UNSUPPORTED;

	if (!delay_to_steps(cfg->tx_delay_ps, &tx_steps) ||
	    !delay_to_steps(cfg->rx_delay_ps, &rx_steps))
		return YT8531C_ERR_RANGE;

	switch (cfg->interface) {
	case YT8531C_IF_RGMII:
		tx_steps = 0u;
		rx_steps = 0u;
		rx_mask = 0u;
		rxc_set = 0u;
		rxc_mask = YT8531C_CHIP_CFG_RXC_DLY_EN;
		break;
	case YT8531C_IF_RGMII_RXID:
		tx_steps = 0u;
		break;
	case YT8531C_IF_RGMII_TXID:
		rx_steps = 0u;
		rx_mask = 0u;
		rxc_set = 0u;
		rxc_mask = YT8531C_CHIP_CFG_RXC_DLY_EN;
		break;
	case YT8531C_IF_RGMII_ID:
		break;
	default:
		return YT8531C_ERR_UNSUPPORTED;
	}

	if (tx_steps == YT8531C_DELAY_KEEP)
		tx_mask = 0u;
	else
		value |= field_prep(YT8531C_RGMII1_TX_DELAY_MASK, tx_steps);

	if (rx_steps == YT8531C_DELAY_KEEP)
		rx_mask = 0u;
	else
		value |= field_prep(YT8531C_RGMII1_RX_DELAY_MASK, rx_steps);

	if (cfg->tx_clock_invert != 0u)
		value |= YT8531C_RGMII1_TX_CLK_INVERT;

	rc = ext_modify(dev, YT8531C_EXT_RGMII_CFG1,
			static_cast<yt_u16>(tx_mask | rx_mask |
					    YT8531C_RGMII1_TX_CLK_INVERT),
			value);
	if (rc != YT8531C_OK)
		return rc;

	if (cfg->interface == static_cast<yt_u8>(YT8531C_IF_RGMII_RXID) ||
	    cfg->interface == static_cast<yt_u8>(YT8531C_IF_RGMII_ID)) {
		if (cfg->rx_clock_delay_en == YT8531C_AUTO) {
			rxc_mask = 0u;
		} else {
			rxc_mask = YT8531C_CHIP_CFG_RXC_DLY_EN;
			rxc_set = (cfg->rx_clock_delay_en != 0u)
					  ? YT8531C_CHIP_CFG_RXC_DLY_EN
					  : 0u;
		}
	}

	if (rxc_mask != 0u) {
		rc = ext_modify(dev, YT8531C_EXT_CHIP_CONFIG, rxc_mask,
				rxc_set);
		if (rc != YT8531C_OK)
			return rc;
	}

	return YT8531C_OK;
}

yt_rc apply_pad_drive(yt8531c_dev *dev)
{
	const yt8531c_config *cfg = &dev->cfg;
	yt_u16 mask = 0u;
	yt_u16 set = 0u;
	yt_u16 rxd;

	if (cfg->rx_clk_drive_strength != YT8531C_AUTO) {
		mask |= YT8531C_PAD_DRIVE_RXC_MASK;
		set |= field_prep(YT8531C_PAD_DRIVE_RXC_MASK,
				  static_cast<yt_u16>(
					  cfg->rx_clk_drive_strength & 7u));
	}

	if (cfg->rx_data_drive_strength != YT8531C_AUTO) {
		rxd = static_cast<yt_u16>(cfg->rx_data_drive_strength & 7u);
		mask |= YT8531C_PAD_DRIVE_RXD_MASK;
		set |= static_cast<yt_u16>(
			((rxd & 0x4u) != 0u ? YT8531C_PAD_DRIVE_RXD_HI : 0u) |
			field_prep(YT8531C_PAD_DRIVE_RXD_LOW_MASK,
						rxd & 0x3u));
	}

	if (cfg->mdio_drive_strength != YT8531C_AUTO) {
		mask |= YT8531C_PAD_DRIVE_MDIO_MASK;
		set |= field_prep(YT8531C_PAD_DRIVE_MDIO_MASK,
				  static_cast<yt_u16>(
					  cfg->mdio_drive_strength & 3u));
	}

	if (cfg->led_drive_strength != YT8531C_AUTO) {
		mask |= YT8531C_PAD_DRIVE_LED_MASK;
		set |= field_prep(YT8531C_PAD_DRIVE_LED_MASK,
				  static_cast<yt_u16>(
					  cfg->led_drive_strength & 3u));
	}

	if (cfg->synce_drive_strength != YT8531C_AUTO) {
		mask |= YT8531C_PAD_DRIVE_SYNCE_MASK;
		set |= field_prep(YT8531C_PAD_DRIVE_SYNCE_MASK,
				  static_cast<yt_u16>(
					  cfg->synce_drive_strength & 3u));
	}

	if (cfg->int_od_enable != YT8531C_AUTO) {
		mask |= YT8531C_PAD_DRIVE_INT_OD_EN;
		if (cfg->int_od_enable != 0u)
			set |= YT8531C_PAD_DRIVE_INT_OD_EN;
	}

	if (cfg->int_active_high != YT8531C_AUTO) {
		mask |= YT8531C_PAD_DRIVE_INT_ACT_HI;
		if (cfg->int_active_high != 0u)
			set |= YT8531C_PAD_DRIVE_INT_ACT_HI;
	}

	if (mask == 0u)
		return YT8531C_OK;

	return ext_modify(dev, YT8531C_EXT_PAD_DRIVE, mask, set);
}

yt_rc apply_clock_output(yt8531c_dev *dev)
{
	const yt8531c_config *cfg = &dev->cfg;
	yt_u16 mask = YT8531C_SYNCE_CFG_ENABLE;
	yt_u16 set = 0u;
	yt_u8 source;

	if (cfg->clk_out == static_cast<yt_u8>(YT8531C_CLK_OUT_OFF))
		return ext_modify(dev, YT8531C_EXT_SYNCE_CFG, mask, 0u);

	mask |= YT8531C_SYNCE_CFG_FREQ_125M | YT8531C_SYNCE_CFG_SRC_MASK |
		YT8531C_SYNCE_CFG_DURING_LINKDOWN;
	set |= YT8531C_SYNCE_CFG_ENABLE;

	if (cfg->clk_out == static_cast<yt_u8>(YT8531C_CLK_OUT_125M)) {
		set |= YT8531C_SYNCE_CFG_FREQ_125M;
		source = (cfg->clk_out_src ==
			  static_cast<yt_u8>(YT8531C_CLK_SRC_AUTO))
				 ? YT8531C_SYNCE_SRC_PLL_125M
				 : cfg->clk_out_src;
	} else {
		source = (cfg->clk_out_src ==
			  static_cast<yt_u8>(YT8531C_CLK_SRC_AUTO))
				 ? YT8531C_SYNCE_SRC_REF_25M
				 : cfg->clk_out_src;
	}

	set |= field_prep(YT8531C_SYNCE_CFG_SRC_MASK, source);

	if (cfg->clk_out_during_link_down != 0u)
		set |= YT8531C_SYNCE_CFG_DURING_LINKDOWN;

	return ext_modify(dev, YT8531C_EXT_SYNCE_CFG, mask, set);
}

yt_rc apply_low_power(yt8531c_dev *dev)
{
	const yt8531c_config *cfg = &dev->cfg;
	yt_rc rc;

	if (cfg->auto_sleep_disabled != 0u) {
		rc = ext_modify(dev, YT8531C_EXT_SLEEP_CTRL,
				YT8531C_SLEEP_CTRL_SLEEP_SW, 0u);
		if (rc != YT8531C_OK)
			return rc;
	}

	if (cfg->keep_pll_enabled != 0u) {
		rc = ext_modify(dev, YT8531C_EXT_CLOCK_GATING,
				YT8531C_CLOCK_GATING_RX_CLK_EN, 0u);
		if (rc != YT8531C_OK)
			return rc;
	}

	return YT8531C_OK;
}

yt_rc apply_misc(yt8531c_dev *dev)
{
	const yt8531c_config *cfg = &dev->cfg;
	yt_u16 mask = 0u;
	yt_u16 set = 0u;
	yt_rc rc;

	set = (cfg->jumbo_enable != 0u) ? YT8531C_MISC_CFG_JUMBO_ENABLE : 0u;
	rc = ext_modify(dev, YT8531C_EXT_MISC_CFG,
			YT8531C_MISC_CFG_JUMBO_ENABLE, set);
	if (rc != YT8531C_OK)
		return rc;

	mask = 0u;
	set = 0u;
	if (cfg->phyaddr0_respond != YT8531C_AUTO) {
		mask |= YT8531C_MDIO_CFG_EN_PHYADDR0;
		if (cfg->phyaddr0_respond != 0u)
			set |= YT8531C_MDIO_CFG_EN_PHYADDR0;
	}

	mask |= YT8531C_MDIO_CFG_EN_BROADCAST |
		YT8531C_MDIO_CFG_BROADCAST_ADDR_MASK;
	if (cfg->broadcast_enable != 0u) {
		set |= YT8531C_MDIO_CFG_EN_BROADCAST;
		set |= field_prep(YT8531C_MDIO_CFG_BROADCAST_ADDR_MASK,
				  cfg->broadcast_addr);
	}

	rc = ext_modify(dev, YT8531C_EXT_MDIO_CFG, mask, set);
	if (rc != YT8531C_OK)
		return rc;

	if (cfg->speed_downgrade_enable != YT8531C_AUTO ||
	    cfg->speed_downgrade_retry != YT8531C_AUTO) {
		mask = 0u;
		set = 0u;

		if (cfg->speed_downgrade_enable != YT8531C_AUTO) {
			mask |= YT8531C_DOWNGRADE_ENABLE;
			if (cfg->speed_downgrade_enable != 0u)
				set |= YT8531C_DOWNGRADE_ENABLE;
		}
		if (cfg->speed_downgrade_retry != YT8531C_AUTO) {
			mask |= YT8531C_DOWNGRADE_RETRY_MASK;
			set |= static_cast<yt_u16>(
				(cfg->speed_downgrade_retry & 7u)
				<< YT8531C_DOWNGRADE_RETRY_SHIFT);
		}

		rc = mii_modify(dev, YT8531C_REG_SPEED_DOWNGRADE, mask, set);
		if (rc != YT8531C_OK)
			return rc;
	}

	return YT8531C_OK;
}

yt_rc apply_eee(yt8531c_dev *dev)
{
	const yt8531c_config *cfg = &dev->cfg;
	yt_u16 ability = 0u;

	if (cfg->eee_enable != 0u) {
		if ((cfg->advertise & YT8531C_MODE_1000FD) != 0u)
			ability |= YT8531C_EEE_ABILITY_1000BASE_T;
		if ((cfg->advertise & YT8531C_MODE_100FD) != 0u)
			ability |= YT8531C_EEE_ABILITY_100BASE_TX;
	}

	return mmd_modify(dev, YT8531C_MMD7, 0x003cu,
			  static_cast<yt_u16>(YT8531C_EEE_ABILITY_1000BASE_T |
					      YT8531C_EEE_ABILITY_100BASE_TX),
			  ability);
}

} /* namespace */

extern "C" {

yt_rc yt8531c_init(yt8531c_dev *dev, const yt8531c_config *cfg,
		   const yt8531c_port *port)
{
	yt8531c_config cfg_copy;
	yt8531c_port port_copy;
	yt_rc rc;

	if (dev == nullptr || cfg == nullptr || port == nullptr)
		return YT8531C_ERR_ARG;

	if (port->mdio_read == nullptr || port->mdio_write == nullptr ||
	    port->delay_us == nullptr)
		return YT8531C_ERR_ARG;

	rc = validate_config(cfg);
	if (rc != YT8531C_OK)
		return rc;

	/*
	 * Copy before clearing: @cfg and @port may alias the members of @dev
	 * when a caller re-initialises an embedded context.
	 */
	cfg_copy = *cfg;
	port_copy = *port;

	yt8531c::detail::mem_zero(dev, sizeof(*dev));

	dev->abi_version = YT8531C_VERSION_MAKE(YT8531C_VERSION_MAJOR,
						YT8531C_VERSION_MINOR,
						YT8531C_VERSION_PATCH);
	dev->struct_size = static_cast<yt_u32>(sizeof(*dev));
	dev->cfg = cfg_copy;
	dev->port = port_copy;
	dev->int_mask = cfg_copy.int_mask;
	dev->magic = YT8531C_DEV_MAGIC;
	dev->ldo_voltage = YT8531C_LDO_VOLTAGE_3V3;

	return YT8531C_OK;
}

yt_rc yt8531c_probe(yt8531c_dev *dev, yt8531c_id *id)
{
	yt_u16 id1 = 0u;
	yt_u16 id2 = 0u;
	yt_u16 chip_config = 0u;
	yt_u32 full_id;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_PHYSID1, &id1);
	if (rc != YT8531C_OK)
		return rc;

	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_PHYSID2, &id2);
	if (rc != YT8531C_OK)
		return rc;

	full_id = (static_cast<yt_u32>(id1) << 16) | id2;

	if (id != nullptr) {
		yt8531c::detail::mem_zero(id, sizeof(*id));
		id->id = full_id;
		id->id1 = id1;
		id->id2 = id2;
		id->model = static_cast<yt_u8>((id2 >> 4) & 0x3fu);
		id->revision = static_cast<yt_u8>(id2 & 0x0fu);
	}

	if (id1 != YT8531C_PHY_ID1_EXPECTED)
		return YT8531C_ERR_ID;

	if (full_id == YT8531C_PHY_ID_YT8531S)
		return YT8531C_ERR_UNSUPPORTED;

	if ((full_id & YT8531C_PHY_ID_MODEL_MASK) !=
	    YT8531C_PHY_ID_MODEL_YT8531)
		return YT8531C_ERR_ID;

	rc = yt8531c::detail::ext_read(dev, YT8531C_EXT_CHIP_CONFIG,
				       &chip_config);
	if (rc != YT8531C_OK)
		return rc;

	dev->chip_id = full_id;
	dev->chip_config = chip_config;
	dev->model = static_cast<yt_u8>((id2 >> 4) & 0x3fu);
	dev->revision = static_cast<yt_u8>(id2 & 0x0fu);
	{
		yt_u8 voltage = static_cast<yt_u8>(
			(chip_config & YT8531C_CHIP_CFG_LDO_VOL_MASK) >>
			YT8531C_CHIP_CFG_LDO_VOL_SHIFT);
		if (voltage > YT8531C_LDO_VOLTAGE_1V8)
			voltage = YT8531C_LDO_VOLTAGE_1V8;
		dev->ldo_voltage = voltage;
	}
	dev->probed = 1u;

	if (id != nullptr) {
		id->chip_config = chip_config;
		id->ldo_voltage = dev->ldo_voltage;
		id->supported = 1u;
	}

	return YT8531C_OK;
}

yt_rc yt8531c_reset(yt8531c_dev *dev)
{
	const yt_u32 timeout_ms =
		(dev != nullptr && dev->cfg.reset_timeout_ms != 0u)
			? dev->cfg.reset_timeout_ms
			: 500u;
	yt_u16 value = 0u;
	yt_u32 waited = 0u;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	if ((dev->port.caps & YT8531C_PORT_CAP_RESET) != 0u &&
	    dev->port.hardware_reset != nullptr) {
		if (dev->port.hardware_reset(dev->port.ctx) < 0)
			return YT8531C_ERR_IO;
		yt8531c::detail::delay_ms(dev, dev->cfg.reset_settle_ms != 0u
						       ? dev->cfg.reset_settle_ms
						       : 20u);
	}

	rc = yt8531c::detail::mii_write(dev, YT8531C_REG_BMCR,
					YT8531C_BMCR_RESET);
	if (rc != YT8531C_OK)
		return rc;

	for (;;) {
		rc = yt8531c::detail::mii_read(dev, YT8531C_REG_BMCR, &value);
		if (rc != YT8531C_OK)
			return rc;
		if ((value & YT8531C_BMCR_RESET) == 0u)
			break;

		if (waited >= timeout_ms)
			return YT8531C_ERR_TIMEOUT;

		yt8531c::detail::delay_ms(dev, 1u);
		++waited;
	}

	yt8531c::detail::delay_ms(dev, 2u);

	rc = yt8531c::detail::ext_read(dev, YT8531C_EXT_CHIP_CONFIG,
				       &dev->chip_config);
	if (rc != YT8531C_OK)
		return rc;

	dev->probed = 0u;
	dev->configured = 0u;
	dev->wol_enabled = 0u;
	dev->link = 0u;
	dev->last_link = 0u;
	dev->speed = YT8531C_SPEED_UNKNOWN;
	dev->last_speed = YT8531C_SPEED_UNKNOWN;

	return yt8531c_probe(dev, nullptr);
}

yt_rc yt8531c_config_init(yt8531c_dev *dev)
{
	yt_u16 scratch = 0u;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	if (dev->probed == 0u) {
		rc = yt8531c_probe(dev, nullptr);
		if (rc != YT8531C_OK)
			return rc;
	}

	rc = apply_rgmii(dev);
	if (rc != YT8531C_OK)
		return rc;

	rc = apply_pad_drive(dev);
	if (rc != YT8531C_OK)
		return rc;

	rc = apply_clock_output(dev);
	if (rc != YT8531C_OK)
		return rc;

	rc = apply_low_power(dev);
	if (rc != YT8531C_OK)
		return rc;

	rc = apply_misc(dev);
	if (rc != YT8531C_OK)
		return rc;

	rc = apply_eee(dev);
	if (rc != YT8531C_OK)
		return rc;

	rc = yt8531c::detail::program_advertisement(dev);
	if (rc != YT8531C_OK)
		return rc;

	/* Disable and clear interrupts before applying the final mask. */
	rc = yt8531c::detail::mii_write(dev, YT8531C_REG_INT_MASK, 0u);
	if (rc != YT8531C_OK)
		return rc;
	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_INT_STATUS, &scratch);
	if (rc != YT8531C_OK)
		return rc;
	rc = yt8531c::detail::mii_write(dev, YT8531C_REG_INT_MASK,
					dev->int_mask);
	if (rc != YT8531C_OK)
		return rc;

	if (dev->cfg.wol_enable != 0u) {
		rc = yt8531c_wol_set(dev, dev->cfg.mac, 1u,
				     dev->cfg.wol_pmeb_mode,
				     dev->cfg.wol_pulse_width);
		if (rc != YT8531C_OK)
			return rc;
	} else {
		rc = yt8531c_wol_set(dev, nullptr, 0u, 0u,
				     dev->cfg.wol_pulse_width);
		if (rc != YT8531C_OK)
			return rc;
	}

	dev->configured = 1u;
	return YT8531C_OK;
}

yt_rc yt8531c_suspend(yt8531c_dev *dev)
{
	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	/* Keep the PHY powered while Wake-on-LAN is armed. */
	if (dev->cfg.wol_enable != 0u || dev->wol_enabled != 0u)
		return YT8531C_OK;

	return yt8531c::detail::mii_modify(dev, YT8531C_REG_BMCR,
					   YT8531C_BMCR_POWER_DOWN,
					   YT8531C_BMCR_POWER_DOWN);
}

yt_rc yt8531c_resume(yt8531c_dev *dev)
{
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	/* Disable auto-sleep so the link comes back deterministically. */
	rc = yt8531c::detail::ext_modify(dev, YT8531C_EXT_SLEEP_CTRL,
					 YT8531C_SLEEP_CTRL_SLEEP_SW, 0u);
	if (rc != YT8531C_OK)
		return rc;

	if (dev->cfg.wol_enable != 0u || dev->wol_enabled != 0u)
		return YT8531C_OK;

	rc = yt8531c::detail::mii_modify(dev, YT8531C_REG_BMCR,
					 YT8531C_BMCR_POWER_DOWN, 0u);
	if (rc != YT8531C_OK)
		return rc;

	yt8531c::detail::delay_ms(dev, 1u);
	return YT8531C_OK;
}

yt_rc yt8531c_set_loopback(yt8531c_dev *dev, yt_u8 mode)
{
	yt_u16 mask = YT8531C_MISC_CFG_REMOTE_LOOPBACK |
		      YT8531C_MISC_CFG_ULDATA_RLOOPBACK;
	yt_u16 set = 0u;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	switch (mode) {
	case YT8531C_LOOPBACK_OFF:
		break;
	case YT8531C_LOOPBACK_DIGITAL:
		/* Make sure a remote loopback is not left enabled. */
		rc = yt8531c::detail::ext_modify(
			dev, YT8531C_EXT_MISC_CFG,
			YT8531C_MISC_CFG_REMOTE_LOOPBACK |
				YT8531C_MISC_CFG_ULDATA_RLOOPBACK,
			0u);
		if (rc != YT8531C_OK)
			return rc;
		return yt8531c::detail::mii_modify(dev, YT8531C_REG_BMCR,
						   YT8531C_BMCR_LOOPBACK,
						   YT8531C_BMCR_LOOPBACK);
	case YT8531C_LOOPBACK_REMOTE:
		set = YT8531C_MISC_CFG_REMOTE_LOOPBACK;
		break;
	case YT8531C_LOOPBACK_REMOTE_UPLOAD:
		set = YT8531C_MISC_CFG_REMOTE_LOOPBACK |
		      YT8531C_MISC_CFG_ULDATA_RLOOPBACK;
		break;
	default:
		return YT8531C_ERR_ARG;
	}

	rc = yt8531c::detail::mii_modify(dev, YT8531C_REG_BMCR,
					 YT8531C_BMCR_LOOPBACK, 0u);
	if (rc != YT8531C_OK)
		return rc;

	return yt8531c::detail::ext_modify(dev, YT8531C_EXT_MISC_CFG, mask,
					   set);
}

yt_rc yt8531c_set_test_mode(yt8531c_dev *dev, yt_u8 mode)
{
	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;
	if (mode > YT8531C_TEST_MODE_4)
		return YT8531C_ERR_ARG;

	return yt8531c::detail::mii_modify(
		dev, YT8531C_REG_CTRL1000, YT8531C_CTRL1000_TEST_MASK,
		static_cast<yt_u16>(static_cast<yt_u16>(mode) << 13));
}

yt_rc yt8531c_set_jumbo(yt8531c_dev *dev, yt_u8 enable)
{
	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;
	if (enable > 1u)
		return YT8531C_ERR_ARG;

	return yt8531c::detail::ext_modify(
		dev, YT8531C_EXT_MISC_CFG, YT8531C_MISC_CFG_JUMBO_ENABLE,
		enable != 0u ? YT8531C_MISC_CFG_JUMBO_ENABLE : 0u);
}

} /* extern "C" */
