/* SPDX-License-Identifier: GPL-3.0-only */

#include "mock_mdio.hpp"

namespace {

constexpr struct {
	yt_u16 addr;
	yt_u16 value;
} kDefaultExt[] = {
	{ YT8531C_EXT_CHIP_CONFIG, 0x0140u },
	{ YT8531C_EXT_RGMII_CFG1, 0x00f1u },
	{ YT8531C_EXT_RGMII_CFG2, 0x0000u },
	{ YT8531C_EXT_MDIO_CFG, 0x00c0u },
	{ YT8531C_EXT_MISC_CFG, 0x0008u },
	{ YT8531C_EXT_WOL_MAC2, 0x0000u },
	{ YT8531C_EXT_WOL_MAC1, 0x0000u },
	{ YT8531C_EXT_WOL_MAC0, 0x0000u },
	{ YT8531C_EXT_WOL_CFG, 0x0002u },
	{ YT8531C_EXT_LED_GENERAL, 0xe000u },
	{ YT8531C_EXT_LED0_CFG, 0x0000u },
	{ YT8531C_EXT_LED1_CFG, 0x0000u },
	{ YT8531C_EXT_LED2_CFG, 0x0000u },
	{ YT8531C_EXT_LED_BLINK, 0x0006u },
	{ YT8531C_EXT_PAD_DRIVE, 0x6bffu },
	{ YT8531C_EXT_SYNCE_CFG, 0x00c8u },
	{ YT8531C_EXT_CLOCK_GATING, 0x1000u },
	{ YT8531C_EXT_SLEEP_CTRL, 0x4000u },
};

constexpr struct {
	yt_u8 devad;
	yt_u16 reg;
	yt_u16 value;
} kDefaultMmd[] = {
	{ YT8531C_MMD3, 0x0001u, 0x0000u },
	{ YT8531C_MMD3, 0x0014u, 0x0006u },
	{ YT8531C_MMD3, 0x0016u, 0x0000u },
	{ YT8531C_MMD7, 0x003cu, 0x0000u },
	{ YT8531C_MMD7, 0x003du, 0x0000u },
};

yt_u16 mmd_devad(yt_u16 ctrl)
{
	return static_cast<yt_u16>(ctrl & YT8531C_MMD_DEVAD_MASK);
}

bool mmd_is_data(yt_u16 ctrl)
{
	return (ctrl & YT8531C_MMD_FUNCTION_MASK) !=
	       YT8531C_MMD_FUNCTION_ADDR;
}

} /* namespace */

void MockMiiIo::reset()
{
	for (unsigned int i = 0; i < 32u; ++i)
		mii_[i] = 0u;

	mii_[0x00] = 0x1140u; /* AN enable, full duplex, 1000 Mbit/s */
	mii_[0x01] = 0x7949u;
	mii_[0x02] = 0x4f51u;
	mii_[0x03] = 0xe91bu;
	mii_[0x04] = 0x01e1u;
	mii_[0x05] = 0x0000u;
	mii_[0x06] = 0x0001u;
	mii_[0x09] = 0x0200u;
	mii_[0x0a] = 0x0000u;
	mii_[0x0d] = 0x0000u;
	mii_[0x0e] = 0x0000u;
	mii_[0x0f] = 0x2000u;
	mii_[0x10] = 0x0062u;
	mii_[0x11] = 0x0000u;
	mii_[0x12] = 0x0000u;
	mii_[0x13] = 0x0000u;
	mii_[0x14] = 0x002cu;
	mii_[0x15] = 0x0000u;
	mii_[0x1e] = 0x0000u;
	mii_[0x1f] = 0x0000u;

	ext_count_ = 0u;
	for (unsigned int i = 0; i < sizeof(kDefaultExt) / sizeof(kDefaultExt[0]);
	     ++i)
		ext_set(kDefaultExt[i].addr, kDefaultExt[i].value);

	ext_addr_cur_ = 0u;

	mmd_count_ = 0u;
	mmd_ctrl_ = 0u;
	mmd_addr_ = 0u;
	for (unsigned int i = 0; i < sizeof(kDefaultMmd) / sizeof(kDefaultMmd[0]);
	     ++i)
		mmd_set(kDefaultMmd[i].devad, kDefaultMmd[i].reg,
			kDefaultMmd[i].value);

	reset_active_ = false;
	reset_stuck = false;
	fail_reads = 0;
	fail_writes = 0;
	fail_mii_reg = -1;
	fail_ext = false;
	delay_calls = 0u;
	delay_us_total = 0u;
	log_count_ = 0u;
}

yt_u16 MockMiiIo::mii_get(yt_u8 reg) const
{
	return (reg < 32u) ? mii_[reg] : 0u;
}

void MockMiiIo::mii_set(yt_u8 reg, yt_u16 value)
{
	if (reg < 32u)
		mii_[reg] = value;
}

yt_u16 MockMiiIo::ext_get(yt_u16 addr) const
{
	for (unsigned int i = 0; i < ext_count_; ++i) {
		if (ext_[i].addr == addr)
			return ext_[i].value;
	}
	return 0u;
}

void MockMiiIo::ext_set(yt_u16 addr, yt_u16 value)
{
	for (unsigned int i = 0; i < ext_count_; ++i) {
		if (ext_[i].addr == addr) {
			ext_[i].value = value;
			return;
		}
	}
	if (ext_count_ < kSparse) {
		ext_[ext_count_].addr = addr;
		ext_[ext_count_].value = value;
		++ext_count_;
	}
}

yt_u16 MockMiiIo::mmd_get(yt_u8 devad, yt_u16 reg) const
{
	for (unsigned int i = 0; i < mmd_count_; ++i) {
		if (mmd_[i].devad == devad && mmd_[i].reg == reg)
			return mmd_[i].value;
	}
	return 0u;
}

void MockMiiIo::mmd_set(yt_u8 devad, yt_u16 reg, yt_u16 value)
{
	for (unsigned int i = 0; i < mmd_count_; ++i) {
		if (mmd_[i].devad == devad && mmd_[i].reg == reg) {
			mmd_[i].value = value;
			return;
		}
	}
	if (mmd_count_ < kSparse) {
		mmd_[mmd_count_].devad = devad;
		mmd_[mmd_count_].reg = reg;
		mmd_[mmd_count_].value = value;
		++mmd_count_;
	}
}

void MockMiiIo::set_link(bool up, yt_u16 speed, bool full_duplex)
{
	if (!up) {
		mii_[0x01] = static_cast<yt_u16>(mii_[0x01] &
						 ~YT8531C_BMSR_LINK_STATUS);
		mii_[0x11] = 0u;
		mii_[0x0a] = 0u;
		return;
	}

	yt_u16 spec = YT8531C_SPEC_STATUS_LINK | YT8531C_SPEC_STATUS_RESOLVED;

	if (full_duplex)
		spec |= YT8531C_SPEC_STATUS_DUPLEX;
	if (speed == YT8531C_SPEED_1000)
		spec |= YT8531C_SPEC_STATUS_SPEED_1000;
	else if (speed == YT8531C_SPEED_100)
		spec |= YT8531C_SPEC_STATUS_SPEED_100;

	mii_[0x11] = spec;
	mii_[0x01] = static_cast<yt_u16>(mii_[0x01] |
					 YT8531C_BMSR_LINK_STATUS);

	if (speed == YT8531C_SPEED_1000)
		mii_[0x0a] = static_cast<yt_u16>(YT8531C_STAT1000_MS_RESOLVED |
						 YT8531C_STAT1000_LP_1000_FULL);
}

void MockMiiIo::set_aneg_complete(bool complete)
{
	if (complete)
		mii_[0x01] |= YT8531C_BMSR_ANEG_COMPLETE;
	else
		mii_[0x01] &= static_cast<yt_u16>(~YT8531C_BMSR_ANEG_COMPLETE);
}

void MockMiiIo::set_master_slave_resolved(bool master)
{
	if (master)
		mii_[0x0a] |= YT8531C_STAT1000_MS_RESOLVED;
	else
		mii_[0x0a] &= static_cast<yt_u16>(
			~YT8531C_STAT1000_MS_RESOLVED);
}

void MockMiiIo::set_mdix(bool mdix)
{
	if (mdix)
		mii_[0x11] |= YT8531C_SPEC_STATUS_MDIX;
	else
		mii_[0x11] &= static_cast<yt_u16>(~YT8531C_SPEC_STATUS_MDIX);
}

void MockMiiIo::set_pause(bool tx, bool rx)
{
	if (tx)
		mii_[0x11] |= YT8531C_SPEC_STATUS_TX_PAUSE;
	else
		mii_[0x11] &= static_cast<yt_u16>(~YT8531C_SPEC_STATUS_TX_PAUSE);
	if (rx)
		mii_[0x11] |= YT8531C_SPEC_STATUS_RX_PAUSE;
	else
		mii_[0x11] &= static_cast<yt_u16>(~YT8531C_SPEC_STATUS_RX_PAUSE);
}

void MockMiiIo::set_interrupt_status(yt_u16 events)
{
	mii_[0x13] = events;
}

int MockMiiIo::read(yt_u8 phy, yt_u8 reg, yt_u16 *value)
{
	if (value == nullptr || phy > 31u || reg > 31u)
		return -1;
	if (fail_reads > 0) {
		--fail_reads;
		return -1;
	}
	if (fail_mii_reg == static_cast<int>(reg))
		return -1;

	switch (reg) {
	case YT8531C_REG_EXT_ADDR:
		*value = ext_addr_cur_;
		return 0;
	case YT8531C_REG_EXT_DATA:
		if (fail_ext)
			return -1;
		*value = ext_get(ext_addr_cur_);
		return 0;
	case YT8531C_REG_MMD_CTRL:
		*value = mmd_ctrl_;
		return 0;
	case YT8531C_REG_MMD_DATA:
		if (!mmd_is_data(mmd_ctrl_)) {
			*value = mmd_addr_;
			return 0;
		}
		*value = mmd_get(static_cast<yt_u8>(mmd_devad(mmd_ctrl_)),
				 mmd_addr_);
		if (mmd_devad(mmd_ctrl_) == YT8531C_MMD3 &&
		    mmd_addr_ == 0x0016u)
			mmd_set(YT8531C_MMD3, mmd_addr_, 0u);
		return 0;
	case YT8531C_REG_INT_STATUS:
		*value = mii_[0x13];
		mii_[0x13] = 0u;
		return 0;
	case YT8531C_REG_STAT1000:
		*value = mii_[0x0a];
		mii_[0x0a] = static_cast<yt_u16>(mii_[0x0a] & 0xff00u);
		return 0;
	case YT8531C_REG_BMCR:
		if (reset_active_) {
			reset_active_ = false;
			*value = static_cast<yt_u16>(mii_[0x00] |
						     YT8531C_BMCR_RESET);
		} else {
			*value = mii_[0x00];
		}
		if (reset_stuck)
			*value |= YT8531C_BMCR_RESET;
		return 0;
	default:
		*value = mii_[reg];
		return 0;
	}
}

int MockMiiIo::write(yt_u8 phy, yt_u8 reg, yt_u16 value)
{
	if (phy > 31u || reg > 31u)
		return -1;
	if (fail_writes > 0) {
		--fail_writes;
		return -1;
	}
	if (fail_mii_reg == static_cast<int>(reg))
		return -1;

	switch (reg) {
	case YT8531C_REG_EXT_ADDR:
		ext_addr_cur_ = value;
		log_access(value, 0u, true, false);
		return 0;
	case YT8531C_REG_EXT_DATA:
		if (fail_ext)
			return -1;
		ext_set(ext_addr_cur_, value);
		log_access(ext_addr_cur_, value, true, true);
		return 0;
	case YT8531C_REG_MMD_CTRL:
		mmd_ctrl_ = value;
		return 0;
	case YT8531C_REG_MMD_DATA:
		if (!mmd_is_data(mmd_ctrl_)) {
			mmd_addr_ = value;
			return 0;
		}
		mmd_set(static_cast<yt_u8>(mmd_devad(mmd_ctrl_)), mmd_addr_,
			value);
		return 0;
	case YT8531C_REG_BMCR:
		mii_[0x00] = static_cast<yt_u16>(value &
						 ~YT8531C_BMCR_RESET);
		if ((value & YT8531C_BMCR_RESET) != 0u)
			reset_active_ = true;
		log_access(reg, value, false, true);
		return 0;
	case YT8531C_REG_INT_STATUS:
		return 0;
	default:
		mii_[reg] = value;
		log_access(reg, value, false, true);
		return 0;
	}
}

void MockMiiIo::clear_log()
{
	log_count_ = 0u;
}

void MockMiiIo::log_access(yt_u16 addr, yt_u16 value, bool is_ext,
			   bool is_write)
{
	if (log_count_ >= kLogSize)
		return;

	log_[log_count_].addr = addr;
	log_[log_count_].value = value;
	log_[log_count_].is_ext = is_ext ? 1u : 0u;
	log_[log_count_].is_write = is_write ? 1u : 0u;
	++log_count_;
}

int MockMiiIo::count_writes(yt_u8 reg) const
{
	int count = 0;

	for (unsigned int i = 0; i < log_count_; ++i) {
		if (log_[i].is_write != 0u && log_[i].is_ext == 0u &&
		    log_[i].addr == reg)
			++count;
	}
	return count;
}

bool MockMiiIo::find_write(yt_u8 reg, yt_u16 *value) const
{
	for (unsigned int i = log_count_; i > 0u; --i) {
		const LogEntry &entry = log_[i - 1u];

		if (entry.is_write != 0u && entry.is_ext == 0u &&
		    entry.addr == reg) {
			if (value != nullptr)
				*value = entry.value;
			return true;
		}
	}
	return false;
}

bool MockMiiIo::find_ext_write(yt_u16 addr, yt_u16 *value) const
{
	for (unsigned int i = log_count_; i > 0u; --i) {
		const LogEntry &entry = log_[i - 1u];

		if (entry.is_write != 0u && entry.is_ext != 0u &&
		    entry.addr == addr) {
			if (value != nullptr)
				*value = entry.value;
			return true;
		}
	}
	return false;
}

namespace {

int mock_read(void *ctx, yt_u8 phy, yt_u8 reg, yt_u16 *value)
{
	return static_cast<MockMiiIo *>(ctx)->read(phy, reg, value);
}

int mock_write(void *ctx, yt_u8 phy, yt_u8 reg, yt_u16 value)
{
	return static_cast<MockMiiIo *>(ctx)->write(phy, reg, value);
}

void mock_delay(void *ctx, yt_u32 us)
{
	MockMiiIo *io = static_cast<MockMiiIo *>(ctx);

	++io->delay_calls;
	io->delay_us_total += us;
}

void mock_log(void *, yt_u8, const char *, yt_u32)
{
}

} /* namespace */

yt8531c_port mock_port(MockMiiIo *io)
{
	yt8531c_port port = YT8531C_PORT_INITIALIZER;

	port.ctx = io;
	port.mdio_read = mock_read;
	port.mdio_write = mock_write;
	port.delay_us = mock_delay;
	port.log = mock_log;
	port.caps = YT8531C_PORT_CAP_LOG;
	return port;
}
