/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * YT8531C core: versions, defaults, raw register access and diagnostics.
 */

#include "yt8531c/yt8531c.h"

#include "yt8531c_internal.hpp"

namespace {

/* LDO current-to-drive-strength mapping (vendor characterisation table). */
struct ldo_current_map {
	yt_u8 voltage;
	yt_u32 microamps;
	yt_u8 strength;
};

constexpr ldo_current_map kLdoCurrentMap[] = {
	{ YT8531C_LDO_VOLTAGE_1V8, 1200u, 0u },
	{ YT8531C_LDO_VOLTAGE_1V8, 2100u, 1u },
	{ YT8531C_LDO_VOLTAGE_1V8, 2700u, 2u },
	{ YT8531C_LDO_VOLTAGE_1V8, 2910u, 3u },
	{ YT8531C_LDO_VOLTAGE_1V8, 3110u, 4u },
	{ YT8531C_LDO_VOLTAGE_1V8, 3600u, 5u },
	{ YT8531C_LDO_VOLTAGE_1V8, 3970u, 6u },
	{ YT8531C_LDO_VOLTAGE_1V8, 4350u, 7u },
	{ YT8531C_LDO_VOLTAGE_3V3, 3070u, 0u },
	{ YT8531C_LDO_VOLTAGE_3V3, 4080u, 1u },
	{ YT8531C_LDO_VOLTAGE_3V3, 4370u, 2u },
	{ YT8531C_LDO_VOLTAGE_3V3, 4680u, 3u },
	{ YT8531C_LDO_VOLTAGE_3V3, 5020u, 4u },
	{ YT8531C_LDO_VOLTAGE_3V3, 5450u, 5u },
	{ YT8531C_LDO_VOLTAGE_3V3, 5740u, 6u },
	{ YT8531C_LDO_VOLTAGE_3V3, 6140u, 7u },
};

constexpr yt_u16 kDumpExtRegs[] = {
	YT8531C_EXT_CHIP_CONFIG, YT8531C_EXT_RGMII_CFG1,
	YT8531C_EXT_RGMII_CFG2,	 YT8531C_EXT_MDIO_CFG,
	YT8531C_EXT_MISC_CFG,	 YT8531C_EXT_WOL_MAC2,
	YT8531C_EXT_WOL_MAC1,	 YT8531C_EXT_WOL_MAC0,
	YT8531C_EXT_WOL_CFG,	 YT8531C_EXT_LED_GENERAL,
	YT8531C_EXT_LED0_CFG,	 YT8531C_EXT_LED1_CFG,
	YT8531C_EXT_LED2_CFG,	 YT8531C_EXT_LED_BLINK,
	YT8531C_EXT_PAD_DRIVE,	 YT8531C_EXT_SYNCE_CFG,
	YT8531C_EXT_CLOCK_GATING, YT8531C_EXT_SLEEP_CTRL,
};

struct mmd_dump_entry {
	yt_u8 devad;
	yt_u8 reg;
};

constexpr mmd_dump_entry kDumpMmdRegs[] = {
	{ YT8531C_MMD3, 0x00u },
	{ YT8531C_MMD3, 0x01u },
	{ YT8531C_MMD3, 0x14u },
	{ YT8531C_MMD3, 0x16u },
	{ YT8531C_MMD7, 0x3cu },
	{ YT8531C_MMD7, 0x3du },
};

constexpr yt_u32 kDumpExtCount =
	sizeof(kDumpExtRegs) / sizeof(kDumpExtRegs[0]);
constexpr yt_u32 kDumpMmdCount =
	sizeof(kDumpMmdRegs) / sizeof(kDumpMmdRegs[0]);

static_assert(kDumpExtCount <= YT8531C_DUMP_EXT_COUNT,
	      "extended register dump table does not fit");
static_assert(kDumpMmdCount <= YT8531C_DUMP_MMD_COUNT,
	      "MMD register dump table does not fit");

} /* namespace */

extern "C" {

yt_u32 yt8531c_version(void)
{
	return YT8531C_VERSION_MAKE(YT8531C_VERSION_MAJOR,
				    YT8531C_VERSION_MINOR,
				    YT8531C_VERSION_PATCH);
}

yt_u32 yt8531c_context_size(void)
{
	return static_cast<yt_u32>(sizeof(yt8531c_dev));
}

void yt8531c_config_default(yt8531c_config *cfg)
{
	if (cfg == nullptr)
		return;

	yt8531c::detail::mem_zero(cfg, sizeof(*cfg));

	cfg->struct_size = static_cast<yt_u32>(sizeof(*cfg));
	cfg->advertise = YT8531C_MODE_DEFAULT;
	cfg->int_mask = YT8531C_IRQ_MASK_DEFAULT;
	cfg->tx_delay_ps = 0u;
	cfg->rx_delay_ps = 0u;
	cfg->phy_addr = 0u;
	cfg->interface = static_cast<yt_u8>(YT8531C_IF_RGMII);
	cfg->reset_timeout_ms = 500u;
	cfg->reset_settle_ms = 20u;
	cfg->auto_sleep_disabled = 0u;
	cfg->keep_pll_enabled = 0u;
	cfg->jumbo_enable = 0u;
	cfg->eee_enable = 0u;
	cfg->master_slave = static_cast<yt_u8>(YT8531C_MS_AUTO);
	cfg->rx_clock_delay_en = YT8531C_AUTO;
	cfg->tx_clock_invert = 0u;
	cfg->tx_clk_adj_enable = 0u;
	cfg->tx_clk_invert_10 = 0u;
	cfg->tx_clk_invert_100 = 0u;
	cfg->tx_clk_invert_1000 = 0u;
	cfg->rx_clk_drive_strength = YT8531C_AUTO;
	cfg->rx_data_drive_strength = YT8531C_AUTO;
	cfg->mdio_drive_strength = YT8531C_AUTO;
	cfg->led_drive_strength = YT8531C_AUTO;
	cfg->synce_drive_strength = YT8531C_AUTO;
	cfg->int_od_enable = YT8531C_AUTO;
	cfg->int_active_high = YT8531C_AUTO;
	cfg->clk_out = static_cast<yt_u8>(YT8531C_CLK_OUT_OFF);
	cfg->clk_out_src = static_cast<yt_u8>(YT8531C_CLK_SRC_AUTO);
	cfg->clk_out_during_link_down = 0u;
	cfg->wol_enable = 0u;
	cfg->wol_pmeb_mode = 0u;
	cfg->wol_pulse_width = YT8531C_WOL_PULSE_168MS;
	cfg->speed_downgrade_enable = YT8531C_AUTO;
	cfg->speed_downgrade_retry = YT8531C_AUTO;
	cfg->phyaddr0_respond = YT8531C_AUTO;
	cfg->broadcast_enable = 0u;
	cfg->broadcast_addr = 0x1fu;
}

yt_rc yt8531c_mii_read(yt8531c_dev *dev, yt_u8 reg, yt_u16 *value)
{
	return yt8531c::detail::mii_read(dev, reg, value);
}

yt_rc yt8531c_mii_write(yt8531c_dev *dev, yt_u8 reg, yt_u16 value)
{
	return yt8531c::detail::mii_write(dev, reg, value);
}

yt_rc yt8531c_ext_read(yt8531c_dev *dev, yt_u16 reg, yt_u16 *value)
{
	return yt8531c::detail::ext_read(dev, reg, value);
}

yt_rc yt8531c_ext_write(yt8531c_dev *dev, yt_u16 reg, yt_u16 value)
{
	return yt8531c::detail::ext_write(dev, reg, value);
}

yt_rc yt8531c_mmd_read(yt8531c_dev *dev, yt_u8 devad, yt_u16 reg,
		       yt_u16 *value)
{
	return yt8531c::detail::mmd_read(dev, devad, reg, value);
}

yt_rc yt8531c_mmd_write(yt8531c_dev *dev, yt_u8 devad, yt_u16 reg,
			yt_u16 value)
{
	return yt8531c::detail::mmd_write(dev, devad, reg, value);
}

yt_rc yt8531c_drive_strength_from_current(yt_u8 ldo_voltage,
					  yt_u32 microamps, yt_u8 *strength)
{
	yt_u8 voltage;

	if (strength == nullptr || ldo_voltage > YT8531C_LDO_VOLTAGE_1V8)
		return YT8531C_ERR_ARG;

	/*
	 * The data sheet only characterises 1.8 V and 3.3 V I/O.  The 2.5 V
	 * setting is closer to the 1.8 V drive characteristics, so it reuses
	 * that table, matching the vendor reference driver.
	 */
	voltage = (ldo_voltage == YT8531C_LDO_VOLTAGE_3V3)
			  ? YT8531C_LDO_VOLTAGE_3V3
			  : YT8531C_LDO_VOLTAGE_1V8;

	for (yt_u32 i = 0u; i < YT8531C_ARRAY_SIZE(kLdoCurrentMap); ++i) {
		if (kLdoCurrentMap[i].voltage == voltage &&
		    kLdoCurrentMap[i].microamps == microamps) {
			*strength = kLdoCurrentMap[i].strength;
			return YT8531C_OK;
		}
	}

	return YT8531C_ERR_RANGE;
}

yt_rc yt8531c_dump(yt8531c_dev *dev, yt8531c_snapshot *dump)
{
	if (dump == nullptr || !yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	yt8531c::detail::mem_zero(dump, sizeof(*dump));

	for (yt_u8 reg = 0u; reg < YT8531C_DUMP_MII_COUNT; ++reg) {
		yt_u16 value = 0xffffu;

		if (yt8531c::detail::mii_read(dev, reg, &value) !=
		    YT8531C_OK)
			value = 0xffffu;
		dump->mii[reg] = value;
	}

	for (yt_u32 i = 0u; i < kDumpExtCount; ++i) {
		yt_u16 value = 0xffffu;

		if (yt8531c::detail::ext_read(dev, kDumpExtRegs[i], &value) !=
		    YT8531C_OK)
			value = 0xffffu;
		dump->ext[i].addr = kDumpExtRegs[i];
		dump->ext[i].value = value;
	}
	dump->ext_count = static_cast<yt_u8>(kDumpExtCount);

	for (yt_u32 i = 0u; i < kDumpMmdCount; ++i) {
		yt_u16 value = 0xffffu;

		if (yt8531c::detail::mmd_read(dev, kDumpMmdRegs[i].devad,
					      kDumpMmdRegs[i].reg,
					      &value) != YT8531C_OK)
			value = 0xffffu;
		dump->mmd[i].devad = kDumpMmdRegs[i].devad;
		dump->mmd[i].reg = kDumpMmdRegs[i].reg;
		dump->mmd[i].value = value;
	}
	dump->mmd_count = static_cast<yt_u8>(kDumpMmdCount);

	return YT8531C_OK;
}

const char *yt8531c_strerror(yt_rc rc)
{
	switch (rc) {
	case YT8531C_OK:
		return "success";
	case YT8531C_ERR_ARG:
		return "invalid argument";
	case YT8531C_ERR_IO:
		return "MDIO transfer failed";
	case YT8531C_ERR_TIMEOUT:
		return "hardware timeout";
	case YT8531C_ERR_ID:
		return "unsupported PHY identification";
	case YT8531C_ERR_UNSUPPORTED:
		return "operation not supported by this PHY";
	case YT8531C_ERR_STATE:
		return "invalid device state";
	case YT8531C_ERR_RANGE:
		return "value out of range";
	case YT8531C_ERR_VERIFY:
		return "register read-back mismatch";
	default:
		return "unknown error";
	}
}

} /* extern "C" */
