/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * YT8531C core: LED configuration.
 *
 * The three LEDs are controlled by one configuration register each
 * (0xa00c + index).  Force mode and blink timing are global and live in
 * LED_GENERAL_CFG (0xa00b) and LED_BLINK_CFG (0xa00f).
 */

#include "yt8531c/yt8531c.h"

#include "yt8531c_internal.hpp"

namespace {

using namespace yt8531c::detail;

yt_rc validate_led_index(yt_u8 index)
{
	return (index > 2u) ? YT8531C_ERR_ARG : YT8531C_OK;
}

yt_u16 triggers_to_bits(yt_u16 triggers)
{
	yt_u16 bits = 0u;

	if ((triggers & YT8531C_LED_TRIG_LINK) != 0u)
		bits |= YT8531C_LED_CFG_10_ON | YT8531C_LED_CFG_100_ON |
			YT8531C_LED_CFG_1000_ON;
	if ((triggers & YT8531C_LED_TRIG_LINK_10) != 0u)
		bits |= YT8531C_LED_CFG_10_ON;
	if ((triggers & YT8531C_LED_TRIG_LINK_100) != 0u)
		bits |= YT8531C_LED_CFG_100_ON;
	if ((triggers & YT8531C_LED_TRIG_LINK_1000) != 0u)
		bits |= YT8531C_LED_CFG_1000_ON;
	if ((triggers & YT8531C_LED_TRIG_FULL_DUPLEX) != 0u)
		bits |= YT8531C_LED_CFG_FDX_ON;
	if ((triggers & YT8531C_LED_TRIG_HALF_DUPLEX) != 0u)
		bits |= YT8531C_LED_CFG_HDX_ON;
	if ((triggers & (YT8531C_LED_TRIG_TX | YT8531C_LED_TRIG_ACTIVITY)) !=
	    0u)
		bits |= YT8531C_LED_CFG_TXACT_BLK;
	if ((triggers & (YT8531C_LED_TRIG_RX | YT8531C_LED_TRIG_ACTIVITY)) !=
	    0u)
		bits |= YT8531C_LED_CFG_RXACT_BLK;
	if ((triggers & (YT8531C_LED_TRIG_TX | YT8531C_LED_TRIG_RX |
			 YT8531C_LED_TRIG_ACTIVITY)) != 0u)
		bits |= YT8531C_LED_CFG_ACT_BLK_IND;
	if ((triggers & YT8531C_LED_TRIG_COLLISION) != 0u)
		bits |= YT8531C_LED_CFG_COL_BLK;

	return bits;
}

yt_u16 bits_to_triggers(yt_u16 bits)
{
	yt_u16 triggers = 0u;
	bool link10 = (bits & YT8531C_LED_CFG_10_ON) != 0u;
	bool link100 = (bits & YT8531C_LED_CFG_100_ON) != 0u;
	bool link1000 = (bits & YT8531C_LED_CFG_1000_ON) != 0u;

	if (link10)
		triggers |= YT8531C_LED_TRIG_LINK_10;
	if (link100)
		triggers |= YT8531C_LED_TRIG_LINK_100;
	if (link1000)
		triggers |= YT8531C_LED_TRIG_LINK_1000;
	if (link10 && link100 && link1000)
		triggers |= YT8531C_LED_TRIG_LINK;

	if ((bits & YT8531C_LED_CFG_FDX_ON) != 0u)
		triggers |= YT8531C_LED_TRIG_FULL_DUPLEX;
	if ((bits & YT8531C_LED_CFG_HDX_ON) != 0u)
		triggers |= YT8531C_LED_TRIG_HALF_DUPLEX;
	if ((bits & (YT8531C_LED_CFG_TXACT_BLK |
		     YT8531C_LED_CFG_ACT_BLK_IND)) != 0u)
		triggers |= YT8531C_LED_TRIG_TX;
	if ((bits & (YT8531C_LED_CFG_RXACT_BLK |
		     YT8531C_LED_CFG_ACT_BLK_IND)) != 0u)
		triggers |= YT8531C_LED_TRIG_RX;
	if ((bits & YT8531C_LED_CFG_COL_BLK) != 0u)
		triggers |= YT8531C_LED_TRIG_COLLISION;

	return triggers;
}

yt_rc apply_force_mode(yt8531c_dev *dev, yt_u8 index, yt_u8 force_mode)
{
	yt_u16 mask;
	yt_u16 set;
	yt_u8 shift = static_cast<yt_u8>(index * 3u);

	if (force_mode == YT8531C_LED_FORCE_AUTO) {
		mask = static_cast<yt_u16>(1u << (2u + shift));
		return ext_modify(dev, YT8531C_EXT_LED_GENERAL, mask, 0u);
	}

	mask = static_cast<yt_u16>((1u << (2u + shift)) |
				   (0x3u << shift));
	set = static_cast<yt_u16>((1u << (2u + shift)) |
				  (static_cast<yt_u16>(force_mode & 0x3u)
				   << shift));
	return ext_modify(dev, YT8531C_EXT_LED_GENERAL, mask, set);
}

yt_rc apply_blink(yt8531c_dev *dev, yt_u8 duty, yt_u8 freq1, yt_u8 freq2)
{
	yt_u16 mask = 0u;
	yt_u16 set = 0u;

	if (duty != YT8531C_AUTO) {
		mask |= YT8531C_LED_BLINK_DUTY_MASK;
		set |= field_prep(YT8531C_LED_BLINK_DUTY_MASK, duty);
	}
	if (freq1 != YT8531C_AUTO) {
		mask |= YT8531C_LED_BLINK_FREQ1_MASK;
		set |= field_prep(YT8531C_LED_BLINK_FREQ1_MASK, freq1);
	}
	if (freq2 != YT8531C_AUTO) {
		mask |= YT8531C_LED_BLINK_FREQ2_MASK;
		set |= field_prep(YT8531C_LED_BLINK_FREQ2_MASK, freq2);
	}

	if (mask == 0u)
		return YT8531C_OK;

	return ext_modify(dev, YT8531C_EXT_LED_BLINK, mask, set);
}

} /* namespace */

extern "C" {

yt_rc yt8531c_led_set(yt8531c_dev *dev, yt_u8 index,
		      const yt8531c_led_cfg *cfg)
{
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev) || cfg == nullptr)
		return YT8531C_ERR_ARG;

	rc = validate_led_index(index);
	if (rc != YT8531C_OK)
		return rc;

	if ((cfg->triggers & static_cast<yt_u16>(~YT8531C_LED_TRIG_ALL)) != 0u)
		return YT8531C_ERR_ARG;

	if (cfg->force_mode != YT8531C_LED_FORCE_AUTO &&
	    cfg->force_mode > YT8531C_LED_FORCE_BLINK1)
		return YT8531C_ERR_ARG;

	if ((cfg->blink_duty != YT8531C_AUTO && cfg->blink_duty > 7u) ||
	    (cfg->blink_freq1 != YT8531C_AUTO && cfg->blink_freq1 > 3u) ||
	    (cfg->blink_freq2 != YT8531C_AUTO && cfg->blink_freq2 > 3u))
		return YT8531C_ERR_ARG;

	rc = ext_modify(
		dev, static_cast<yt_u16>(YT8531C_EXT_LED0_CFG + index),
		YT8531C_LED_CFG_TRIGGER_MASK, triggers_to_bits(cfg->triggers));
	if (rc != YT8531C_OK)
		return rc;

	rc = apply_blink(dev, cfg->blink_duty, cfg->blink_freq1,
			 cfg->blink_freq2);
	if (rc != YT8531C_OK)
		return rc;

	return apply_force_mode(dev, index, cfg->force_mode);
}

yt_rc yt8531c_led_get(yt8531c_dev *dev, yt_u8 index, yt8531c_led_cfg *cfg)
{
	yt_u16 led_cfg = 0u;
	yt_u16 general = 0u;
	yt_u16 blink = 0u;
	yt_u16 mask;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev) || cfg == nullptr)
		return YT8531C_ERR_ARG;

	rc = validate_led_index(index);
	if (rc != YT8531C_OK)
		return rc;

	yt8531c::detail::mem_zero(cfg, sizeof(*cfg));

	rc = ext_read(dev, static_cast<yt_u16>(YT8531C_EXT_LED0_CFG + index),
		      &led_cfg);
	if (rc != YT8531C_OK)
		return rc;

	rc = ext_read(dev, YT8531C_EXT_LED_GENERAL, &general);
	if (rc != YT8531C_OK)
		return rc;

	rc = ext_read(dev, YT8531C_EXT_LED_BLINK, &blink);
	if (rc != YT8531C_OK)
		return rc;

	cfg->triggers = bits_to_triggers(led_cfg);

	mask = static_cast<yt_u16>(1u << (2u + index * 3u));
	if ((general & mask) != 0u)
		cfg->force_mode = static_cast<yt_u8>(
			(general >> (index * 3u)) & 0x3u);
	else
		cfg->force_mode = YT8531C_LED_FORCE_AUTO;

	cfg->blink_duty = static_cast<yt_u8>(
		(blink & YT8531C_LED_BLINK_DUTY_MASK) >>
		YT8531C_LED_BLINK_DUTY_SHIFT);
	cfg->blink_freq1 = static_cast<yt_u8>(
		(blink & YT8531C_LED_BLINK_FREQ1_MASK) >>
		YT8531C_LED_BLINK_FREQ1_SHIFT);
	cfg->blink_freq2 = static_cast<yt_u8>(
		(blink & YT8531C_LED_BLINK_FREQ2_MASK) >>
		YT8531C_LED_BLINK_FREQ2_SHIFT);

	return YT8531C_OK;
}

yt_rc yt8531c_led_blink_set(yt8531c_dev *dev, yt_u8 duty, yt_u8 freq1,
			    yt_u8 freq2)
{
	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	if ((duty != YT8531C_AUTO && duty > 7u) ||
	    (freq1 != YT8531C_AUTO && freq1 > 3u) ||
	    (freq2 != YT8531C_AUTO && freq2 > 3u))
		return YT8531C_ERR_ARG;

	return apply_blink(dev, duty, freq1, freq2);
}

} /* extern "C" */
