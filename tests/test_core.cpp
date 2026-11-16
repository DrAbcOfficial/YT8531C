/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Core tests: defaults, validation, identification, reset and configuration.
 */

#include "test_common.hpp"

namespace {

void expect_init_error(const Fixture &fx, yt_rc expected)
{
	yt8531c_dev dev;
	yt_rc rc = yt8531c_init(&dev, &fx.cfg, &fx.port);

	YT_CHECK_EQ(rc, expected);
}

} /* namespace */

void test_config_defaults(void)
{
	yt8531c_config cfg;

	yt8531c_config_default(&cfg);

	YT_CHECK_EQ(cfg.struct_size, sizeof(yt8531c_config));
	YT_CHECK_EQ(cfg.advertise, YT8531C_MODE_DEFAULT);
	YT_CHECK_EQ(cfg.int_mask, YT8531C_IRQ_MASK_DEFAULT);
	YT_CHECK_EQ(cfg.phy_addr, 0);
	YT_CHECK_EQ(cfg.interface, YT8531C_IF_RGMII);
	YT_CHECK_EQ(cfg.reset_timeout_ms, 500);
	YT_CHECK_EQ(cfg.reset_settle_ms, 20);
	YT_CHECK_EQ(cfg.clk_out, YT8531C_CLK_OUT_OFF);
	YT_CHECK_EQ(cfg.clk_out_src, YT8531C_CLK_SRC_AUTO);
	YT_CHECK_EQ(cfg.rx_clk_drive_strength, YT8531C_AUTO);
	YT_CHECK_EQ(cfg.int_od_enable, YT8531C_AUTO);
	YT_CHECK_EQ(cfg.master_slave, YT8531C_MS_AUTO);
	YT_CHECK_EQ(cfg.wol_pulse_width, YT8531C_WOL_PULSE_168MS);
	YT_CHECK_EQ(cfg.broadcast_addr, 0x1f);

	YT_CHECK_EQ(yt8531c_version(), YT8531C_VERSION);
	YT_CHECK(yt8531c_context_size() == sizeof(yt8531c_dev));
	YT_CHECK(yt8531c_context_size() >= 64u);
}

void test_init_validation(void)
{
	Fixture fx;
	yt8531c_dev dev;
	yt8531c_config cfg;
	yt8531c_port port = fx.port;

	YT_CHECK_EQ(yt8531c_init(nullptr, &fx.cfg, &fx.port), YT8531C_ERR_ARG);
	YT_CHECK_EQ(yt8531c_init(&dev, nullptr, &fx.port), YT8531C_ERR_ARG);
	YT_CHECK_EQ(yt8531c_init(&dev, &fx.cfg, nullptr), YT8531C_ERR_ARG);

	port.mdio_read = nullptr;
	YT_CHECK_EQ(yt8531c_init(&dev, &fx.cfg, &port), YT8531C_ERR_ARG);
	port = fx.port;
	port.delay_us = nullptr;
	YT_CHECK_EQ(yt8531c_init(&dev, &fx.cfg, &port), YT8531C_ERR_ARG);

	yt8531c_config_default(&cfg);
	cfg.phy_addr = 32;
	YT_CHECK_EQ(yt8531c_init(&dev, &cfg, &fx.port), YT8531C_ERR_ARG);

	yt8531c_config_default(&cfg);
	cfg.interface = 9;
	YT_CHECK_EQ(yt8531c_init(&dev, &cfg, &fx.port), YT8531C_ERR_ARG);

	yt8531c_config_default(&cfg);
	cfg.tx_delay_ps = 200;
	YT_CHECK_EQ(yt8531c_init(&dev, &cfg, &fx.port), YT8531C_ERR_RANGE);

	yt8531c_config_default(&cfg);
	cfg.rx_delay_ps = 2400;
	YT_CHECK_EQ(yt8531c_init(&dev, &cfg, &fx.port), YT8531C_ERR_RANGE);

	yt8531c_config_default(&cfg);
	cfg.rx_clk_drive_strength = 8;
	YT_CHECK_EQ(yt8531c_init(&dev, &cfg, &fx.port), YT8531C_ERR_ARG);

	yt8531c_config_default(&cfg);
	cfg.mdio_drive_strength = 4;
	YT_CHECK_EQ(yt8531c_init(&dev, &cfg, &fx.port), YT8531C_ERR_ARG);

	yt8531c_config_default(&cfg);
	cfg.clk_out = 3;
	YT_CHECK_EQ(yt8531c_init(&dev, &cfg, &fx.port), YT8531C_ERR_ARG);

	yt8531c_config_default(&cfg);
	cfg.int_mask = 0x0010;
	YT_CHECK_EQ(yt8531c_init(&dev, &cfg, &fx.port), YT8531C_OK);
	YT_CHECK_EQ(dev.magic, YT8531C_DEV_MAGIC);
	YT_CHECK_EQ(dev.abi_version, YT8531C_VERSION);

	expect_init_error(fx, YT8531C_OK);
}

void test_probe_identification(void)
{
	Fixture fx;
	yt8531c_id id;

	YT_CHECK_EQ(yt8531c_probe(&fx.dev, &id), YT8531C_OK);
	YT_CHECK_EQ(id.id, YT8531C_PHY_ID_YT8531);
	YT_CHECK_EQ(id.id1, 0x4f51);
	YT_CHECK_EQ(id.id2, 0xe91b);
	YT_CHECK_EQ(id.model, 0x11);
	YT_CHECK_EQ(id.revision, 0x0b);
	YT_CHECK_EQ(id.ldo_voltage, YT8531C_LDO_VOLTAGE_3V3);
	YT_CHECK_EQ(id.chip_config, 0x0140);
	YT_CHECK_EQ(id.supported, 1);
	YT_CHECK_EQ(fx.dev.chip_id, 0x4f51e91bu);
	YT_CHECK_EQ(fx.dev.probed, 1);

	fx.mock.ext_set(YT8531C_EXT_CHIP_CONFIG, 0x0160u);
	YT_CHECK_EQ(fx.probe(), YT8531C_OK);
	YT_CHECK_EQ(fx.dev.ldo_voltage, YT8531C_LDO_VOLTAGE_1V8);

	fx.mock.mii_set(YT8531C_REG_PHYSID2, 0xe91au);
	YT_CHECK_EQ(fx.probe(), YT8531C_ERR_UNSUPPORTED);

	fx.mock.mii_set(YT8531C_REG_PHYSID2, 0xe920u);
	YT_CHECK_EQ(fx.probe(), YT8531C_ERR_ID);

	fx.mock.mii_set(YT8531C_REG_PHYSID1, 0x1234u);
	YT_CHECK_EQ(fx.probe(), YT8531C_ERR_ID);

	fx.mock.mii_set(YT8531C_REG_PHYSID1, 0x4f51u);
	fx.mock.fail_mii_reg = YT8531C_REG_PHYSID1;
	YT_CHECK_EQ(fx.probe(), YT8531C_ERR_IO);
}

void test_reset(void)
{
	Fixture fx;

	fx.cfg.reset_timeout_ms = 5;
	fx.reload();
	YT_CHECK_EQ(fx.probe(), YT8531C_OK);
	YT_CHECK_EQ(yt8531c_reset(&fx.dev), YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_BMCR) & YT8531C_BMCR_RESET, 0);
	YT_CHECK(fx.mock.delay_calls > 0);
	YT_CHECK_EQ(fx.dev.configured, 0);
	YT_CHECK_EQ(fx.dev.probed, 1);

	fx.mock.reset_stuck = true;
	YT_CHECK_EQ(yt8531c_reset(&fx.dev), YT8531C_ERR_TIMEOUT);

	fx.mock.reset_stuck = false;
	fx.mock.fail_writes = 1;
	YT_CHECK_EQ(yt8531c_reset(&fx.dev), YT8531C_ERR_IO);
}

void test_config_init_rgmii(void)
{
	Fixture fx;

	fx.cfg.interface = YT8531C_IF_RGMII_ID;
	fx.cfg.tx_delay_ps = 1500;
	fx.cfg.rx_delay_ps = 750;
	fx.cfg.rx_clock_delay_en = 1;
	fx.cfg.tx_clock_invert = 1;
	fx.reload();

	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);
	YT_CHECK_EQ(fx.dev.configured, 1);

	yt_u16 rgmii1 = fx.mock.ext_get(YT8531C_EXT_RGMII_CFG1);
	YT_CHECK_EQ(field(YT8531C_RGMII1_TX_DELAY_MASK, rgmii1), 10);
	YT_CHECK_EQ(field(YT8531C_RGMII1_RX_DELAY_MASK, rgmii1), 5);
	YT_CHECK_EQ((rgmii1 & YT8531C_RGMII1_TX_CLK_INVERT) != 0u, 1);
	YT_CHECK_EQ(field(YT8531C_RGMII1_FE_TX_DELAY_MASK, rgmii1), 0x0f);

	yt_u16 chip = fx.mock.ext_get(YT8531C_EXT_CHIP_CONFIG);
	YT_CHECK_EQ((chip & YT8531C_CHIP_CFG_RXC_DLY_EN) != 0u, 1);
	YT_CHECK_EQ((chip & YT8531C_CHIP_CFG_EN_LDO) != 0u, 1);

	/* Default advertisement and 1000BASE-T control. */
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_ADVERTISE), 0x0de1u);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_CTRL1000),
		    YT8531C_CTRL1000_ADV_1000_FULL);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_INT_MASK),
		    YT8531C_IRQ_MASK_DEFAULT);
}

void test_config_init_plain_rgmii(void)
{
	Fixture fx;

	fx.cfg.interface = YT8531C_IF_RGMII;
	fx.cfg.tx_delay_ps = 1500;
	fx.cfg.rx_delay_ps = 1500;
	fx.cfg.rx_clock_delay_en = 1;
	fx.reload();

	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);

	yt_u16 rgmii1 = fx.mock.ext_get(YT8531C_EXT_RGMII_CFG1);
	YT_CHECK_EQ(field(YT8531C_RGMII1_TX_DELAY_MASK, rgmii1), 0);
	YT_CHECK_EQ(field(YT8531C_RGMII1_RX_DELAY_MASK, rgmii1), 0);

	yt_u16 chip = fx.mock.ext_get(YT8531C_EXT_CHIP_CONFIG);
	YT_CHECK_EQ((chip & YT8531C_CHIP_CFG_RXC_DLY_EN) != 0u, 0);
}

void test_config_init_txid(void)
{
	Fixture fx;

	fx.cfg.interface = YT8531C_IF_RGMII_TXID;
	fx.cfg.tx_delay_ps = 600;
	fx.cfg.rx_delay_ps = 1500;
	fx.reload();

	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);

	yt_u16 rgmii1 = fx.mock.ext_get(YT8531C_EXT_RGMII_CFG1);
	YT_CHECK_EQ(field(YT8531C_RGMII1_TX_DELAY_MASK, rgmii1), 4);
	YT_CHECK_EQ(field(YT8531C_RGMII1_RX_DELAY_MASK, rgmii1), 0);

	yt_u16 chip = fx.mock.ext_get(YT8531C_EXT_CHIP_CONFIG);
	YT_CHECK_EQ((chip & YT8531C_CHIP_CFG_RXC_DLY_EN) != 0u, 0);

	fx.cfg.interface = YT8531C_IF_NONE;
	fx.reload();
	YT_CHECK_EQ(fx.probe(), YT8531C_OK);
	YT_CHECK_EQ(yt8531c_config_init(&fx.dev), YT8531C_ERR_UNSUPPORTED);
}

void test_config_init_pad_drive(void)
{
	Fixture fx;

	fx.cfg.rx_clk_drive_strength = 5;
	fx.cfg.rx_data_drive_strength = 3;
	fx.cfg.mdio_drive_strength = 2;
	fx.cfg.led_drive_strength = 1;
	fx.cfg.synce_drive_strength = 3;
	fx.cfg.int_od_enable = 1;
	fx.cfg.int_active_high = 1;
	fx.reload();

	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);

	yt_u16 pad = fx.mock.ext_get(YT8531C_EXT_PAD_DRIVE);
	YT_CHECK_EQ(field(YT8531C_PAD_DRIVE_RXC_MASK, pad), 5);
	YT_CHECK_EQ(field(YT8531C_PAD_DRIVE_RXD_LOW_MASK, pad), 3);
	YT_CHECK_EQ((pad & YT8531C_PAD_DRIVE_RXD_HI) != 0u, 0);
	YT_CHECK_EQ(field(YT8531C_PAD_DRIVE_MDIO_MASK, pad), 2);
	YT_CHECK_EQ(field(YT8531C_PAD_DRIVE_LED_MASK, pad), 1);
	YT_CHECK_EQ(field(YT8531C_PAD_DRIVE_SYNCE_MASK, pad), 3);
	YT_CHECK_EQ((pad & YT8531C_PAD_DRIVE_INT_OD_EN) != 0u, 1);
	YT_CHECK_EQ((pad & YT8531C_PAD_DRIVE_INT_ACT_HI) != 0u, 1);
}

void test_config_init_clock_output(void)
{
	Fixture fx;

	fx.cfg.clk_out = YT8531C_CLK_OUT_125M;
	fx.reload();
	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);
	yt_u16 synce = fx.mock.ext_get(YT8531C_EXT_SYNCE_CFG);
	YT_CHECK_EQ((synce & YT8531C_SYNCE_CFG_ENABLE) != 0u, 1);
	YT_CHECK_EQ((synce & YT8531C_SYNCE_CFG_FREQ_125M) != 0u, 1);
	YT_CHECK_EQ(field(YT8531C_SYNCE_CFG_SRC_MASK, synce),
		    YT8531C_SYNCE_SRC_PLL_125M);

	fx.cfg.clk_out = YT8531C_CLK_OUT_25M;
	fx.cfg.clk_out_src = YT8531C_CLK_SRC_REF_25M;
	fx.cfg.clk_out_during_link_down = 1;
	fx.reload();
	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);
	synce = fx.mock.ext_get(YT8531C_EXT_SYNCE_CFG);
	YT_CHECK_EQ((synce & YT8531C_SYNCE_CFG_ENABLE) != 0u, 1);
	YT_CHECK_EQ((synce & YT8531C_SYNCE_CFG_FREQ_125M) != 0u, 0);
	YT_CHECK_EQ(field(YT8531C_SYNCE_CFG_SRC_MASK, synce),
		    YT8531C_SYNCE_SRC_REF_25M);
	YT_CHECK_EQ((synce & YT8531C_SYNCE_CFG_DURING_LINKDOWN) != 0u, 1);

	fx.cfg.clk_out = YT8531C_CLK_OUT_OFF;
	fx.reload();
	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);
	synce = fx.mock.ext_get(YT8531C_EXT_SYNCE_CFG);
	YT_CHECK_EQ((synce & YT8531C_SYNCE_CFG_ENABLE) != 0u, 0);
}

void test_config_init_misc(void)
{
	Fixture fx;

	fx.mock.ext_set(YT8531C_EXT_SLEEP_CTRL, 0xc000u);
	fx.mock.ext_set(YT8531C_EXT_CLOCK_GATING, 0x1000u);
	fx.cfg.auto_sleep_disabled = 1;
	fx.cfg.keep_pll_enabled = 1;
	fx.cfg.jumbo_enable = 1;
	fx.cfg.phyaddr0_respond = 0;
	fx.cfg.broadcast_enable = 1;
	fx.cfg.broadcast_addr = 7;
	fx.cfg.speed_downgrade_enable = 1;
	fx.cfg.speed_downgrade_retry = 5;
	fx.reload();

	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);

	yt_u16 sleep = fx.mock.ext_get(YT8531C_EXT_SLEEP_CTRL);
	YT_CHECK_EQ((sleep & YT8531C_SLEEP_CTRL_SLEEP_SW) != 0u, 0);
	YT_CHECK_EQ((sleep & YT8531C_SLEEP_CTRL_PLLON_SLP) != 0u, 1);

	yt_u16 gating = fx.mock.ext_get(YT8531C_EXT_CLOCK_GATING);
	YT_CHECK_EQ((gating & YT8531C_CLOCK_GATING_RX_CLK_EN) != 0u, 0);

	yt_u16 misc = fx.mock.ext_get(YT8531C_EXT_MISC_CFG);
	YT_CHECK_EQ((misc & YT8531C_MISC_CFG_JUMBO_ENABLE) != 0u, 1);

	yt_u16 mdio_cfg = fx.mock.ext_get(YT8531C_EXT_MDIO_CFG);
	YT_CHECK_EQ((mdio_cfg & YT8531C_MDIO_CFG_EN_PHYADDR0) != 0u, 0);
	YT_CHECK_EQ((mdio_cfg & YT8531C_MDIO_CFG_EN_BROADCAST) != 0u, 1);
	YT_CHECK_EQ(field(YT8531C_MDIO_CFG_BROADCAST_ADDR_MASK, mdio_cfg), 7);

	yt_u16 downgrade = fx.mock.mii_get(YT8531C_REG_SPEED_DOWNGRADE);
	YT_CHECK_EQ((downgrade & YT8531C_DOWNGRADE_ENABLE) != 0u, 1);
	YT_CHECK_EQ(field(YT8531C_DOWNGRADE_RETRY_MASK, downgrade), 5);
}

void test_config_init_wol(void)
{
	Fixture fx;
	const yt_u8 mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };

	for (unsigned int i = 0; i < 6u; ++i)
		fx.cfg.mac[i] = mac[i];
	fx.cfg.wol_enable = 1;
	fx.cfg.wol_pmeb_mode = 1;
	fx.cfg.wol_pulse_width = YT8531C_WOL_PULSE_672MS;
	fx.reload();

	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);
	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_WOL_MAC2), 0x0211u);
	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_WOL_MAC1), 0x2233u);
	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_WOL_MAC0), 0x4455u);

	yt_u16 cfg = fx.mock.ext_get(YT8531C_EXT_WOL_CFG);
	YT_CHECK_EQ((cfg & YT8531C_WOL_CFG_ENABLE) != 0u, 1);
	YT_CHECK_EQ((cfg & YT8531C_WOL_CFG_PMEB_INTB_SEL) != 0u, 1);
	YT_CHECK_EQ(field(YT8531C_WOL_CFG_PULSE_WIDTH_MASK, cfg),
		    YT8531C_WOL_PULSE_672MS);

	YT_CHECK_EQ((fx.mock.mii_get(YT8531C_REG_INT_MASK) &
		     YT8531C_IRQ_WOL) != 0u,
		    1);
}

void test_dump(void)
{
	Fixture fx;
	yt8531c_snapshot dump;

	YT_CHECK_EQ(fx.probe(), YT8531C_OK);
	YT_CHECK_EQ(yt8531c_dump(&fx.dev, &dump), YT8531C_OK);
	YT_CHECK_EQ(dump.mii[YT8531C_REG_PHYSID1], 0x4f51);
	YT_CHECK_EQ(dump.mii[YT8531C_REG_PHYSID2], 0xe91b);
	YT_CHECK_EQ(dump.ext_count, 18);
	YT_CHECK_EQ(dump.mmd_count, 6);
	YT_CHECK_EQ(dump.ext[0].addr, YT8531C_EXT_CHIP_CONFIG);
	YT_CHECK_EQ(dump.ext[0].value, 0x0140);
	YT_CHECK_EQ(dump.mmd[4].devad, YT8531C_MMD7);
	YT_CHECK_EQ(dump.mmd[4].reg, 0x3c);

	fx.mock.fail_ext = true;
	YT_CHECK_EQ(yt8531c_dump(&fx.dev, &dump), YT8531C_OK);
	YT_CHECK_EQ(dump.ext[0].value, 0xffff);
}
