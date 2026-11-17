/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * YT8531C core: Wake-on-LAN magic packet support.
 */

#include "yt8531c/yt8531c.h"

#include "yt8531c_internal.hpp"

namespace {

using namespace yt8531c::detail;

constexpr yt_u16 kWolMacRegs[3] = {
	YT8531C_EXT_WOL_MAC2, /* MAC bytes 0..1 */
	YT8531C_EXT_WOL_MAC1, /* MAC bytes 2..3 */
	YT8531C_EXT_WOL_MAC0, /* MAC bytes 4..5 */
};

} /* namespace */

extern "C" {

yt_rc yt8531c_wol_set(yt8531c_dev *dev, const yt_u8 mac[6], yt_u8 enable,
		      yt_u8 pmeb_mode, yt_u8 pulse_width)
{
	yt_u16 mask;
	yt_u16 set;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;
	if (enable > 1u || pmeb_mode > 1u ||
	    pulse_width > YT8531C_WOL_PULSE_672MS)
		return YT8531C_ERR_ARG;

	if (enable != 0u) {
		if (mac == nullptr || !mac_is_valid(mac))
			return YT8531C_ERR_ARG;

		for (yt_u8 i = 0u; i < 3u; ++i) {
			yt_u16 value = static_cast<yt_u16>(
				(static_cast<yt_u16>(mac[i * 2u]) << 8) |
				mac[i * 2u + 1u]);

			rc = ext_write(dev, kWolMacRegs[i], value);
			if (rc != YT8531C_OK)
				return rc;
		}
	}

	/*
	 * EXT_0xA00A bit0 selects the PME_N waveform: 1 = level triggered,
	 * 0 = pulse triggered with the width from bits 2:1.  The shared
	 * INT_N pin only supports the level waveform, so the pulse width is
	 * only meaningful on a dedicated PME_N pin.
	 */
	mask = static_cast<yt_u16>(
		YT8531C_WOL_CFG_ENABLE | YT8531C_WOL_CFG_PMEB_INTB_SEL |
		YT8531C_WOL_CFG_PULSE_WIDTH_MASK | YT8531C_WOL_CFG_TYPE_LEVEL);
	set = 0u;

	if (enable != 0u) {
		set |= YT8531C_WOL_CFG_ENABLE;
		set |= field_prep(YT8531C_WOL_CFG_PULSE_WIDTH_MASK,
				  pulse_width);
		if (pmeb_mode != 0u)
			set |= YT8531C_WOL_CFG_PMEB_INTB_SEL;
		else
			set |= YT8531C_WOL_CFG_TYPE_LEVEL;
	}

	rc = ext_modify(dev, YT8531C_EXT_WOL_CFG, mask, set);
	if (rc != YT8531C_OK)
		return rc;

	/* Arm or disarm the WOL interrupt without disturbing other events. */
	rc = mii_modify(dev, YT8531C_REG_INT_MASK, YT8531C_IRQ_WOL,
			enable != 0u ? YT8531C_IRQ_WOL : 0u);
	if (rc != YT8531C_OK)
		return rc;

	dev->wol_enabled = enable;
	return YT8531C_OK;
}

yt_rc yt8531c_wol_get(yt8531c_dev *dev, yt_u8 mac[6], yt_u8 *enable,
		      yt_u8 *pmeb_mode, yt_u8 *pulse_width)
{
	yt_u16 cfg = 0u;
	yt_u16 value;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	rc = ext_read(dev, YT8531C_EXT_WOL_CFG, &cfg);
	if (rc != YT8531C_OK)
		return rc;

	if (mac != nullptr) {
		for (yt_u8 i = 0u; i < 3u; ++i) {
			rc = ext_read(dev, kWolMacRegs[i], &value);
			if (rc != YT8531C_OK)
				return rc;
			mac[i * 2u] = static_cast<yt_u8>(value >> 8);
			mac[i * 2u + 1u] = static_cast<yt_u8>(value & 0xffu);
		}
	}

	if (enable != nullptr)
		*enable =
			((cfg & YT8531C_WOL_CFG_ENABLE) != 0u) ? 1u : 0u;
	if (pmeb_mode != nullptr)
		*pmeb_mode =
			((cfg & YT8531C_WOL_CFG_PMEB_INTB_SEL) != 0u) ? 1u
								      : 0u;
	if (pulse_width != nullptr)
		*pulse_width = static_cast<yt_u8>(
			(cfg & YT8531C_WOL_CFG_PULSE_WIDTH_MASK) >>
			YT8531C_WOL_CFG_PULSE_WIDTH_SHIFT);

	return YT8531C_OK;
}

} /* extern "C" */
