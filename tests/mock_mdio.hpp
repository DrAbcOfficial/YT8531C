/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Register-level mock of a YT8531C on a clause 22 MDIO bus.
 *
 * The mock implements the small amount of chip behaviour the core relies on:
 * the extended register address/data window, the MMD indirect access window,
 * read-clear interrupt/status registers and a self-clearing software reset.
 * It also records writes so tests can assert the exact programming sequence.
 */

#ifndef YT8531C_MOCK_MDIO_HPP
#define YT8531C_MOCK_MDIO_HPP

#include "yt8531c/yt8531c.h"

class MockMiiIo {
public:
	static constexpr unsigned int kLogSize = 512;
	static constexpr unsigned int kSparse = 64;

	struct LogEntry {
		yt_u16 addr;
		yt_u16 value;
		yt_u8 is_ext;
		yt_u8 is_write;
	};

	MockMiiIo() { reset(); }

	void reset();

	int read(yt_u8 phy, yt_u8 reg, yt_u16 *value);
	int write(yt_u8 phy, yt_u8 reg, yt_u16 value);

	/* Direct register access for test setup and assertions. */
	yt_u16 mii_get(yt_u8 reg) const;
	void mii_set(yt_u8 reg, yt_u16 value);
	yt_u16 ext_get(yt_u16 addr) const;
	void ext_set(yt_u16 addr, yt_u16 value);
	yt_u16 mmd_get(yt_u8 devad, yt_u16 reg) const;
	void mmd_set(yt_u8 devad, yt_u16 reg, yt_u16 value);

	/* Convenience chip-state helpers. */
	void set_link(bool up, yt_u16 speed, bool full_duplex);
	void set_aneg_complete(bool complete);
	void set_master_slave_resolved(bool master);
	void set_mdix(bool mdix);
	void set_pause(bool tx, bool rx);
	void set_interrupt_status(yt_u16 events);

	/* Access log. */
	void clear_log();
	unsigned int log_count() const { return log_count_; }
	const LogEntry &log(unsigned int index) const { return log_[index]; }
	int count_writes(yt_u8 reg) const;
	bool find_write(yt_u8 reg, yt_u16 *value) const;
	bool find_ext_write(yt_u16 addr, yt_u16 *value) const;

	/* Fault injection and reset behaviour. */
	int fail_reads;
	int fail_writes;
	int fail_mii_reg; /* -1 disables */
	bool fail_ext;
	bool reset_stuck;
	unsigned int delay_calls;
	yt_u64 delay_us_total;

private:
	yt_u16 mii_[32];
	struct ExtEntry {
		yt_u16 addr;
		yt_u16 value;
	};
	ExtEntry ext_[MockMiiIo::kSparse];
	unsigned int ext_count_;
	yt_u16 ext_addr_cur_;
	struct MmEntry {
		yt_u8 devad;
		yt_u16 reg;
		yt_u16 value;
	};
	MmEntry mmd_[MockMiiIo::kSparse];
	unsigned int mmd_count_;
	yt_u16 mmd_ctrl_;
	yt_u16 mmd_addr_;
	bool reset_active_;
	LogEntry log_[MockMiiIo::kLogSize];
	unsigned int log_count_;

	void log_access(yt_u16 addr, yt_u16 value, bool is_ext, bool is_write);
};

/* Build a yt8531c_port that forwards to @io. */
yt8531c_port mock_port(MockMiiIo *io);

#endif /* YT8531C_MOCK_MDIO_HPP */
