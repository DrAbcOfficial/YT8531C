/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Shared test fixture: a mock MDIO bus plus an initialised device context.
 */

#ifndef YT8531C_TEST_COMMON_HPP
#define YT8531C_TEST_COMMON_HPP

#include "mock_mdio.hpp"
#include "test_framework.hpp"
#include "yt8531c/yt8531c.h"

struct Fixture {
	MockMiiIo mock;
	yt8531c_config cfg;
	yt8531c_port port;
	yt8531c_dev dev;

	Fixture()
	{
		mock.reset();
		yt8531c_config_default(&cfg);
		port = mock_port(&mock);
		(void)yt8531c_init(&dev, &cfg, &port);
	}

	/* Re-create the context with the current configuration. */
	void reload()
	{
		port = mock_port(&mock);
		(void)yt8531c_init(&dev, &cfg, &port);
	}

	yt_rc probe() { return yt8531c_probe(&dev, nullptr); }

	yt_rc init_board()
	{
		yt_rc rc = probe();

		if (rc != YT8531C_OK)
			return rc;
		return yt8531c_config_init(&dev);
	}
};

/* Field extractor for assertions. */
inline yt_u16 field(yt_u16 mask, yt_u16 value)
{
	yt_u16 shifted = static_cast<yt_u16>(value & mask);
	yt_u16 m = mask;
	unsigned int shift = 0;

	while (m != 0u && (m & 1u) == 0u) {
		m = static_cast<yt_u16>(m >> 1);
		++shift;
	}
	return static_cast<yt_u16>(shift == 0u ? shifted : shifted >> shift);
}

#endif /* YT8531C_TEST_COMMON_HPP */
