/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Link tests: advertisement, forced links, status decoding and counters.
 */

#include "test_common.hpp"

void test_aneg(void)
{
	Fixture fx;

	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);
	YT_CHECK_EQ(yt8531c_config_aneg(&fx.dev), YT8531C_OK);

	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_ADVERTISE), 0x0de1u);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_CTRL1000),
		    YT8531C_CTRL1000_ADV_1000_FULL);

	yt_u16 bmcr = fx.mock.mii_get(YT8531C_REG_BMCR);
	YT_CHECK_EQ((bmcr & YT8531C_BMCR_ANEG_ENABLE) != 0u, 1);
	YT_CHECK_EQ((bmcr & YT8531C_BMCR_RESTART_ANEG) != 0u, 1);
	YT_CHECK_EQ((bmcr & YT8531C_BMCR_ISOLATE) != 0u, 0);
	YT_CHECK_EQ(fx.dev.configured, 1);

	fx.cfg.master_slave = YT8531C_MS_PREFER_MASTER;
	fx.reload();
	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);
	YT_CHECK_EQ(yt8531c_config_aneg(&fx.dev), YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_CTRL1000),
		    static_cast<yt_u16>(YT8531C_CTRL1000_ADV_1000_FULL |
					YT8531C_CTRL1000_PREFER_MASTER));

	fx.cfg.master_slave = YT8531C_MS_FORCE_SLAVE;
	fx.reload();
	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);
	YT_CHECK_EQ(yt8531c_config_aneg(&fx.dev), YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_CTRL1000),
		    static_cast<yt_u16>(YT8531C_CTRL1000_ADV_1000_FULL |
					YT8531C_CTRL1000_MASTER_SLAVE_EN));

	fx.cfg.advertise = YT8531C_MODE_1000HD;
	fx.cfg.master_slave = YT8531C_MS_AUTO;
	fx.reload();
	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);
	YT_CHECK_EQ(yt8531c_config_aneg(&fx.dev), YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_CTRL1000),
		    YT8531C_CTRL1000_ADV_1000_HALF);
}

void test_force_and_status(void)
{
	Fixture fx;
	yt8531c_status status;
	yt_u8 done = 0;

	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);

	YT_CHECK_EQ(yt8531c_force_link(&fx.dev, YT8531C_SPEED_100,
				       YT8531C_DUPLEX_FULL),
		    YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_BMCR), 0x2100u);

	YT_CHECK_EQ(yt8531c_force_link(&fx.dev, YT8531C_SPEED_1000,
				       YT8531C_DUPLEX_HALF),
		    YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_BMCR), 0x0040u);

	YT_CHECK_EQ(yt8531c_force_link(&fx.dev, YT8531C_SPEED_10,
				       YT8531C_DUPLEX_HALF),
		    YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_BMCR), 0x0000u);

	YT_CHECK_EQ(yt8531c_force_link(&fx.dev, YT8531C_SPEED_10,
				       YT8531C_DUPLEX_FULL),
		    YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_BMCR), 0x0100u);

	YT_CHECK_EQ(yt8531c_force_link(&fx.dev, 25, YT8531C_DUPLEX_FULL),
		    YT8531C_ERR_ARG);
	YT_CHECK_EQ(yt8531c_force_link(&fx.dev, YT8531C_SPEED_100, 7),
		    YT8531C_ERR_ARG);

	/* Restoring auto-negotiation goes through config_aneg(). */
	YT_CHECK_EQ(yt8531c_force_link(&fx.dev, YT8531C_SPEED_UNKNOWN,
				       YT8531C_DUPLEX_UNKNOWN),
		    YT8531C_OK);
	YT_CHECK_EQ((fx.mock.mii_get(YT8531C_REG_BMCR) &
		     YT8531C_BMCR_ANEG_ENABLE) != 0u,
		    1);

	YT_CHECK_EQ(yt8531c_aneg_done(&fx.dev, &done), YT8531C_OK);
	YT_CHECK_EQ(done, 0);
	fx.mock.set_aneg_complete(true);
	YT_CHECK_EQ(yt8531c_aneg_done(&fx.dev, &done), YT8531C_OK);
	YT_CHECK_EQ(done, 1);
	YT_CHECK_EQ(yt8531c_aneg_done(&fx.dev, nullptr), YT8531C_ERR_ARG);

	/* Linked at 1000 Mbit/s, full duplex, with pause and MDIX. */
	fx.mock.set_aneg_complete(true);
	fx.mock.set_link(true, YT8531C_SPEED_1000, true);
	fx.mock.set_mdix(true);
	fx.mock.set_pause(true, false);

	YT_CHECK_EQ(yt8531c_read_status(&fx.dev, &status), YT8531C_OK);
	YT_CHECK_EQ(status.link, 1);
	YT_CHECK_EQ(status.speed, YT8531C_SPEED_1000);
	YT_CHECK_EQ(status.duplex, YT8531C_DUPLEX_FULL);
	YT_CHECK_EQ(status.autoneg_complete, 1);
	YT_CHECK_EQ(status.mdix, 1);
	YT_CHECK_EQ(status.tx_pause, 1);
	YT_CHECK_EQ(status.rx_pause, 0);
	YT_CHECK_EQ(status.master_slave, YT8531C_MS_MASTER);
	YT_CHECK_EQ(status.link_changed, 1);
	YT_CHECK_EQ(fx.dev.link, 1);
	YT_CHECK_EQ(fx.dev.speed, YT8531C_SPEED_1000);

	YT_CHECK_EQ(yt8531c_read_status(&fx.dev, &status), YT8531C_OK);
	YT_CHECK_EQ(status.link_changed, 0);

	fx.mock.set_link(false, 0, false);
	YT_CHECK_EQ(yt8531c_read_status(&fx.dev, &status), YT8531C_OK);
	YT_CHECK_EQ(status.link, 0);
	YT_CHECK_EQ(status.link_changed, 1);
	YT_CHECK_EQ(status.speed, YT8531C_SPEED_UNKNOWN);
	YT_CHECK_EQ(status.duplex, YT8531C_DUPLEX_UNKNOWN);
	YT_CHECK_EQ(status.mdix, 2);

	/* 100 Mbit/s half duplex without auto-negotiation. */
	fx.mock.set_link(true, YT8531C_SPEED_100, false);
	fx.mock.set_aneg_complete(false);
	YT_CHECK_EQ(yt8531c_read_status(&fx.dev, &status), YT8531C_OK);
	YT_CHECK_EQ(status.speed, YT8531C_SPEED_100);
	YT_CHECK_EQ(status.duplex, YT8531C_DUPLEX_HALF);
	YT_CHECK_EQ(status.master_slave, YT8531C_MS_UNKNOWN);

	fx.mock.fail_reads = 1;
	YT_CHECK_EQ(yt8531c_read_status(&fx.dev, &status), YT8531C_ERR_IO);
	YT_CHECK_EQ(yt8531c_read_status(&fx.dev, nullptr), YT8531C_ERR_ARG);
}

void test_features_and_counters(void)
{
	Fixture fx;
	yt8531c_features features;
	yt8531c_counters counters;

	YT_CHECK_EQ(fx.probe(), YT8531C_OK);
	YT_CHECK_EQ(yt8531c_get_features(&fx.dev, &features), YT8531C_OK);
	YT_CHECK_EQ((features.modes & YT8531C_MODE_1000FD) != 0u, 1);
	YT_CHECK_EQ((features.modes & YT8531C_MODE_100FD) != 0u, 1);
	YT_CHECK_EQ((features.modes & YT8531C_MODE_10HD) != 0u, 1);
	YT_CHECK_EQ((features.modes & YT8531C_MODE_1000HD) != 0u, 0);
	YT_CHECK_EQ((features.flags & YT8531C_FEAT_AUTONEG) != 0u, 1);
	YT_CHECK_EQ((features.flags & YT8531C_FEAT_TP) != 0u, 1);
	YT_CHECK_EQ((features.flags & YT8531C_FEAT_MDIX_AUTO) != 0u, 1);
	YT_CHECK_EQ((features.flags & YT8531C_FEAT_EEE_1000) != 0u, 1);
	YT_CHECK_EQ((features.flags & YT8531C_FEAT_EEE_100) != 0u, 1);
	YT_CHECK_EQ((features.flags & YT8531C_FEAT_WOL) != 0u, 1);
	YT_CHECK_EQ(yt8531c_get_features(&fx.dev, nullptr), YT8531C_ERR_ARG);

	fx.mock.mii_set(YT8531C_REG_RX_ERR_COUNT, 0x0012u);
	fx.mock.mii_set(YT8531C_REG_STAT1000, 0x0109u);
	fx.mock.mmd_set(YT8531C_MMD3, 0x0016u, 3u);

	YT_CHECK_EQ(yt8531c_get_counters(&fx.dev, &counters), YT8531C_OK);
	YT_CHECK_EQ(counters.rx_errors, 0x12);
	YT_CHECK_EQ(counters.idle_errors, 9);
	YT_CHECK_EQ(counters.eee_wake_errors, 3);

	YT_CHECK_EQ(yt8531c_get_counters(&fx.dev, &counters), YT8531C_OK);
	YT_CHECK_EQ(counters.eee_wake_errors, 0);
	YT_CHECK_EQ(counters.idle_errors, 0);

	YT_CHECK_EQ(yt8531c_get_counters(&fx.dev, nullptr), YT8531C_ERR_ARG);
}
