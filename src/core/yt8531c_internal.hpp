/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * Internal helpers shared by the YT8531C core translation units.
 *
 * Nothing in here includes a standard library header: the core is compiled for
 * a Linux kernel module, a freestanding userspace library and cross targets.
 */

#ifndef YT8531C_INTERNAL_HPP
#define YT8531C_INTERNAL_HPP

#include "yt8531c/yt8531c.h"

namespace yt8531c {
namespace detail {

inline void mem_zero(void *ptr, yt_size len)
{
	yt_u8 *p = static_cast<yt_u8 *>(ptr);

	while (len-- != 0u)
		*p++ = 0u;
}

inline bool is_ready(const yt8531c_dev *dev)
{
	return dev != nullptr && dev->magic == YT8531C_DEV_MAGIC &&
	       dev->port.mdio_read != nullptr &&
	       dev->port.mdio_write != nullptr;
}

inline yt_rc mii_read(yt8531c_dev *dev, yt_u8 reg, yt_u16 *value)
{
	yt_u16 tmp = 0u;

	if (!is_ready(dev) || value == nullptr)
		return YT8531C_ERR_ARG;

	if (dev->port.mdio_read(dev->port.ctx, dev->cfg.phy_addr,
				static_cast<yt_u8>(reg & 0x1fu), &tmp) < 0)
		return YT8531C_ERR_IO;

	*value = tmp;
	return YT8531C_OK;
}

inline yt_rc mii_write(yt8531c_dev *dev, yt_u8 reg, yt_u16 value)
{
	if (!is_ready(dev))
		return YT8531C_ERR_ARG;

	if (dev->port.mdio_write(dev->port.ctx, dev->cfg.phy_addr,
				 static_cast<yt_u8>(reg & 0x1fu), value) < 0)
		return YT8531C_ERR_IO;

	return YT8531C_OK;
}

inline yt_rc mii_modify(yt8531c_dev *dev, yt_u8 reg, yt_u16 mask,
			yt_u16 set, yt_u16 *new_value = nullptr)
{
	yt_u16 old = 0u;
	yt_u16 updated;
	yt_rc rc;

	rc = mii_read(dev, reg, &old);
	if (rc != YT8531C_OK)
		return rc;

	updated = static_cast<yt_u16>((old & static_cast<yt_u16>(~mask)) |
				      (set & mask));
	if (updated == old) {
		if (new_value != nullptr)
			*new_value = old;
		return YT8531C_OK;
	}

	rc = mii_write(dev, reg, updated);
	if (rc == YT8531C_OK && new_value != nullptr)
		*new_value = updated;
	return rc;
}

inline yt_rc ext_read(yt8531c_dev *dev, yt_u16 reg, yt_u16 *value)
{
	yt_rc rc;

	if (value == nullptr)
		return YT8531C_ERR_ARG;

	rc = mii_write(dev, YT8531C_REG_EXT_ADDR, reg);
	if (rc != YT8531C_OK)
		return rc;

	return mii_read(dev, YT8531C_REG_EXT_DATA, value);
}

inline yt_rc ext_write(yt8531c_dev *dev, yt_u16 reg, yt_u16 value)
{
	yt_rc rc;

	rc = mii_write(dev, YT8531C_REG_EXT_ADDR, reg);
	if (rc != YT8531C_OK)
		return rc;

	return mii_write(dev, YT8531C_REG_EXT_DATA, value);
}

inline yt_rc ext_modify(yt8531c_dev *dev, yt_u16 reg, yt_u16 mask,
			yt_u16 set, yt_u16 *new_value = nullptr)
{
	yt_u16 old = 0u;
	yt_u16 updated;
	yt_rc rc;

	rc = ext_read(dev, reg, &old);
	if (rc != YT8531C_OK)
		return rc;

	updated = static_cast<yt_u16>((old & static_cast<yt_u16>(~mask)) |
				      (set & mask));
	if (updated == old) {
		if (new_value != nullptr)
			*new_value = old;
		return YT8531C_OK;
	}

	rc = ext_write(dev, reg, updated);
	if (rc == YT8531C_OK && new_value != nullptr)
		*new_value = updated;
	return rc;
}

inline yt_rc mmd_read(yt8531c_dev *dev, yt_u8 devad, yt_u16 reg,
		      yt_u16 *value)
{
	yt_rc rc;

	if (value == nullptr || devad > 31u)
		return YT8531C_ERR_ARG;

	rc = mii_write(dev, YT8531C_REG_MMD_CTRL,
		       static_cast<yt_u16>(YT8531C_MMD_FUNCTION_ADDR |
					   (devad & YT8531C_MMD_DEVAD_MASK)));
	if (rc != YT8531C_OK)
		return rc;

	rc = mii_write(dev, YT8531C_REG_MMD_DATA, reg);
	if (rc != YT8531C_OK)
		return rc;

	rc = mii_write(dev, YT8531C_REG_MMD_CTRL,
		       static_cast<yt_u16>(YT8531C_MMD_FUNCTION_DATA |
					   (devad & YT8531C_MMD_DEVAD_MASK)));
	if (rc != YT8531C_OK)
		return rc;

	return mii_read(dev, YT8531C_REG_MMD_DATA, value);
}

inline yt_rc mmd_write(yt8531c_dev *dev, yt_u8 devad, yt_u16 reg,
		       yt_u16 value)
{
	yt_rc rc;

	if (devad > 31u)
		return YT8531C_ERR_ARG;

	rc = mii_write(dev, YT8531C_REG_MMD_CTRL,
		       static_cast<yt_u16>(YT8531C_MMD_FUNCTION_ADDR |
					   (devad & YT8531C_MMD_DEVAD_MASK)));
	if (rc != YT8531C_OK)
		return rc;

	rc = mii_write(dev, YT8531C_REG_MMD_DATA, reg);
	if (rc != YT8531C_OK)
		return rc;

	rc = mii_write(dev, YT8531C_REG_MMD_CTRL,
		       static_cast<yt_u16>(YT8531C_MMD_FUNCTION_DATA |
					   (devad & YT8531C_MMD_DEVAD_MASK)));
	if (rc != YT8531C_OK)
		return rc;

	return mii_write(dev, YT8531C_REG_MMD_DATA, value);
}

inline yt_rc mmd_modify(yt8531c_dev *dev, yt_u8 devad, yt_u16 reg,
			yt_u16 mask, yt_u16 set)
{
	yt_u16 old = 0u;
	yt_rc rc;

	rc = mmd_read(dev, devad, reg, &old);
	if (rc != YT8531C_OK)
		return rc;

	old = static_cast<yt_u16>((old & static_cast<yt_u16>(~mask)) |
				  (set & mask));
	return mmd_write(dev, devad, reg, old);
}

inline void delay_us(yt8531c_dev *dev, yt_u32 us)
{
	if (dev->port.delay_us != nullptr && us != 0u)
		dev->port.delay_us(dev->port.ctx, us);
}

inline void delay_ms(yt8531c_dev *dev, yt_u32 ms)
{
	delay_us(dev, ms * 1000u);
}

inline void log_msg(yt8531c_dev *dev, yt_u8 level, const char *msg)
{
	yt_u32 len = 0u;

	if ((dev->port.caps & YT8531C_PORT_CAP_LOG) == 0u ||
	    dev->port.log == nullptr || msg == nullptr)
		return;

	while (msg[len] != '\0')
		++len;
	dev->port.log(dev->port.ctx, level, msg, len);
}

/* Extract the right-aligned value of a contiguous bit field. */
inline yt_u16 field_get(yt_u16 mask, yt_u16 value)
{
	yt_u16 shifted = static_cast<yt_u16>(value & mask);
	yt_u16 m = mask;
	unsigned int shift = 0u;

	while ((m & 0x0001u) == 0u && m != 0u) {
		m = static_cast<yt_u16>(m >> 1);
		++shift;
	}

	return static_cast<yt_u16>(shift == 0u ? shifted
					      : (shifted >> shift));
}

/* Place a value into the position described by a contiguous bit mask. */
inline yt_u16 field_prep(yt_u16 mask, yt_u16 value)
{
	yt_u16 m = mask;
	unsigned int shift = 0u;

	while ((m & 0x0001u) == 0u && m != 0u) {
		m = static_cast<yt_u16>(m >> 1);
		++shift;
	}

	return static_cast<yt_u16>((static_cast<yt_u32>(value) << shift) &
				   mask);
}

/* Number of 150 ps RGMII delay steps for a requested delay in picoseconds. */
constexpr yt_u16 kRgmiiDelayMaxSteps = 15u;

inline bool delay_to_steps(yt_u16 ps, yt_u16 *steps)
{
	if (ps == YT8531C_DELAY_KEEP) {
		*steps = YT8531C_DELAY_KEEP;
		return true;
	}
	if (ps > YT8531C_RGMII_DELAY_MAX_PS)
		return false;
	if ((ps % YT8531C_RGMII_DELAY_STEP_PS) != 0u)
		return false;

	*steps = static_cast<yt_u16>(ps / YT8531C_RGMII_DELAY_STEP_PS);
	return true;
}

#if defined(__GNUC__) && !defined(_WIN32)
#define YT8531C_INTERNAL_API __attribute__((visibility("hidden")))
#else
#define YT8531C_INTERNAL_API
#endif

/*
 * Program the auto-negotiation advertisement registers from
 * yt8531c_config::advertise.  Defined in yt8531c_link.cpp and shared with the
 * initialisation path; it never restarts auto-negotiation.
 */
YT8531C_INTERNAL_API yt_rc program_advertisement(yt8531c_dev *dev);

inline bool mac_is_valid(const yt_u8 mac[6])
{
	bool all_zero = true;
	bool all_ones = true;

	for (yt_u8 i = 0u; i < 6u; ++i) {
		if (mac[i] != 0x00u)
			all_zero = false;
		if (mac[i] != 0xffu)
			all_ones = false;
	}

	return !all_zero && !all_ones;
}

} /* namespace detail */
} /* namespace yt8531c */

#endif /* YT8531C_INTERNAL_HPP */
