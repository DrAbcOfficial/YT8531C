/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Peripheral tests: LEDs, Wake-on-LAN, interrupts, EEE, loopback and power.
 */

#include "test_common.hpp"

void test_led(void)
{
	Fixture fx;
	yt8531c_led_cfg cfg;
	yt8531c_led_cfg readback;

	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);

	yt8531c_led_cfg zero = {};
	cfg = zero;
	cfg.triggers = static_cast<yt_u16>(
		YT8531C_LED_TRIG_LINK | YT8531C_LED_TRIG_ACTIVITY |
		YT8531C_LED_TRIG_FULL_DUPLEX | YT8531C_LED_TRIG_COLLISION);
	cfg.force_mode = YT8531C_LED_FORCE_ON;
	cfg.blink_duty = YT8531C_LED_DUTY_75;
	cfg.blink_freq1 = YT8531C_LED_BLINK_8HZ;
	cfg.blink_freq2 = YT8531C_LED_BLINK_16HZ;

	YT_CHECK_EQ(yt8531c_led_set(&fx.dev, 0, &cfg), YT8531C_OK);
	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_LED0_CFG), 0x3678u);

	yt_u16 general = fx.mock.ext_get(YT8531C_EXT_LED_GENERAL);
	YT_CHECK_EQ((general & YT8531C_LED_GEN_FORCE_EN(0)) != 0u, 1);
	YT_CHECK_EQ(field(YT8531C_LED_GEN_FORCE_MODE_MASK(0), general),
		    YT8531C_LED_FORCE_ON);
	YT_CHECK_EQ((general & 0xe000u), 0xe000u);

	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_LED_BLINK), 0x002eu);

	YT_CHECK_EQ(yt8531c_led_get(&fx.dev, 0, &readback), YT8531C_OK);
	YT_CHECK_EQ(readback.triggers, 0x02dfu);
	YT_CHECK_EQ(readback.force_mode, YT8531C_LED_FORCE_ON);
	YT_CHECK_EQ(readback.blink_duty, YT8531C_LED_DUTY_75);
	YT_CHECK_EQ(readback.blink_freq1, YT8531C_LED_BLINK_8HZ);
	YT_CHECK_EQ(readback.blink_freq2, YT8531C_LED_BLINK_16HZ);

	YT_CHECK_EQ(yt8531c_led_set(&fx.dev, 3, &cfg), YT8531C_ERR_ARG);
	YT_CHECK_EQ(yt8531c_led_get(&fx.dev, 3, &readback), YT8531C_ERR_ARG);

	cfg.triggers = 0x4000u;
	YT_CHECK_EQ(yt8531c_led_set(&fx.dev, 0, &cfg), YT8531C_ERR_ARG);

	cfg.triggers = YT8531C_LED_TRIG_LINK_100;
	cfg.force_mode = YT8531C_LED_FORCE_AUTO;
	YT_CHECK_EQ(yt8531c_led_set(&fx.dev, 1, &cfg), YT8531C_OK);
	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_LED1_CFG),
		    static_cast<yt_u16>(YT8531C_LED_CFG_100_ON));

	yt_u16 general2 = fx.mock.ext_get(YT8531C_EXT_LED_GENERAL);
	YT_CHECK_EQ((general2 & YT8531C_LED_GEN_FORCE_EN(1)) != 0u, 0);

	YT_CHECK_EQ(yt8531c_led_blink_set(&fx.dev, YT8531C_LED_DUTY_50,
					  YT8531C_LED_BLINK_2HZ,
					  YT8531C_LED_BLINK_2HZ),
		    YT8531C_OK);
	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_LED_BLINK), 0x0000u);
}

void test_wol(void)
{
	Fixture fx;
	const yt_u8 mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
	yt_u8 read_mac[6] = {};
	yt_u8 enable = 0;
	yt_u8 pmeb = 0;
	yt_u8 pulse = 0;
	const yt_u8 zero_mac[6] = {};

	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);

	YT_CHECK_EQ(yt8531c_wol_set(&fx.dev, mac, 1, 1,
				    YT8531C_WOL_PULSE_672MS),
		    YT8531C_OK);
	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_WOL_MAC2), 0x0211u);
	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_WOL_MAC1), 0x2233u);
	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_WOL_MAC0), 0x4455u);
	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_WOL_CFG), 0x004fu);
	YT_CHECK_EQ((fx.mock.mii_get(YT8531C_REG_INT_MASK) &
		     YT8531C_IRQ_WOL) != 0u,
		    1);
	YT_CHECK_EQ(fx.dev.wol_enabled, 1);

	YT_CHECK_EQ(yt8531c_wol_get(&fx.dev, read_mac, &enable, &pmeb, &pulse),
		    YT8531C_OK);
	YT_CHECK_EQ(enable, 1);
	YT_CHECK_EQ(pmeb, 1);
	YT_CHECK_EQ(pulse, YT8531C_WOL_PULSE_672MS);
	for (unsigned int i = 0; i < 6u; ++i)
		YT_CHECK_EQ(read_mac[i], mac[i]);

	YT_CHECK_EQ(yt8531c_wol_set(&fx.dev, mac, 0, 0, 0),
		    YT8531C_OK);
	YT_CHECK_EQ((fx.mock.ext_get(YT8531C_EXT_WOL_CFG) &
		     YT8531C_WOL_CFG_ENABLE) != 0u,
		    0);
	YT_CHECK_EQ((fx.mock.mii_get(YT8531C_REG_INT_MASK) &
		     YT8531C_IRQ_WOL) != 0u,
		    0);
	YT_CHECK_EQ(fx.dev.wol_enabled, 0);

	YT_CHECK_EQ(yt8531c_wol_set(&fx.dev, zero_mac, 1, 0, 0),
		    YT8531C_ERR_ARG);
	YT_CHECK_EQ(yt8531c_wol_set(&fx.dev, mac, 1, 0, 4),
		    YT8531C_ERR_ARG);
	YT_CHECK_EQ(yt8531c_wol_set(&fx.dev, mac, 2, 0, 0),
		    YT8531C_ERR_ARG);
}

void test_interrupts(void)
{
	Fixture fx;
	yt_u16 mask = 0;
	yt_u16 events = 0;

	YT_CHECK_EQ(yt8531c_irq_default_mask(&mask), YT8531C_OK);
	YT_CHECK_EQ(mask, YT8531C_IRQ_MASK_DEFAULT);
	YT_CHECK_EQ(yt8531c_irq_default_mask(nullptr), YT8531C_ERR_ARG);

	YT_CHECK_EQ(yt8531c_irq_set_mask(&fx.dev, YT8531C_IRQ_LINK_UP),
		    YT8531C_OK);
	YT_CHECK_EQ(yt8531c_irq_get_mask(&fx.dev, &mask), YT8531C_OK);
	YT_CHECK_EQ(mask, YT8531C_IRQ_LINK_UP);
	YT_CHECK_EQ(fx.dev.int_mask, YT8531C_IRQ_LINK_UP);
	YT_CHECK_EQ(yt8531c_irq_set_mask(&fx.dev, 0x0004u), YT8531C_ERR_ARG);
	YT_CHECK_EQ(yt8531c_irq_set_mask(&fx.dev, 0xffffu), YT8531C_ERR_ARG);

	fx.mock.set_interrupt_status(
		static_cast<yt_u16>(YT8531C_IRQ_LINK_UP |
				    YT8531C_IRQ_LINK_DOWN));
	YT_CHECK_EQ(yt8531c_irq_read(&fx.dev, &events), YT8531C_OK);
	YT_CHECK_EQ(events, static_cast<yt_u16>(YT8531C_IRQ_LINK_UP |
						YT8531C_IRQ_LINK_DOWN));
	YT_CHECK_EQ(yt8531c_irq_read(&fx.dev, &events), YT8531C_OK);
	YT_CHECK_EQ(events, 0);

	fx.mock.set_interrupt_status(YT8531C_IRQ_WOL);
	YT_CHECK_EQ(yt8531c_irq_clear(&fx.dev), YT8531C_OK);
	YT_CHECK_EQ(yt8531c_irq_read(&fx.dev, &events), YT8531C_OK);
	YT_CHECK_EQ(events, 0);

	fx.mock.fail_reads = 1;
	YT_CHECK_EQ(yt8531c_irq_read(&fx.dev, &events), YT8531C_ERR_IO);
}

void test_eee(void)
{
	Fixture fx;
	yt8531c_eee eee;

	YT_CHECK_EQ(fx.probe(), YT8531C_OK);
	YT_CHECK_EQ(yt8531c_eee_get(&fx.dev, &eee), YT8531C_OK);
	YT_CHECK_EQ(eee.capabilities, 0x0006u);
	YT_CHECK_EQ(eee.local_ability, 0u);
	YT_CHECK_EQ(eee.lp_ability, 0u);

	YT_CHECK_EQ(yt8531c_eee_set(&fx.dev, YT8531C_EEE_ABILITY_1000BASE_T),
		    YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mmd_get(YT8531C_MMD7, 0x003cu), 0x0004u);

	YT_CHECK_EQ(yt8531c_eee_get(&fx.dev, &eee), YT8531C_OK);
	YT_CHECK_EQ(eee.local_ability, 0x0004u);

	YT_CHECK_EQ(yt8531c_eee_set(&fx.dev, 0x0008u), YT8531C_ERR_ARG);
	YT_CHECK_EQ(yt8531c_eee_set(&fx.dev, 0x0006u), YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mmd_get(YT8531C_MMD7, 0x003cu), 0x0006u);
	YT_CHECK_EQ(yt8531c_eee_get(&fx.dev, nullptr), YT8531C_ERR_ARG);

	/* config_init advertises both EEE speeds when enabled. */
	fx.cfg.eee_enable = 1;
	fx.reload();
	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mmd_get(YT8531C_MMD7, 0x003cu),
		    static_cast<yt_u16>(YT8531C_EEE_ABILITY_1000BASE_T |
					YT8531C_EEE_ABILITY_100BASE_TX));
}

void test_loopback_test_mode(void)
{
	Fixture fx;
	yt_u16 value = 0;

	YT_CHECK_EQ(fx.probe(), YT8531C_OK);

	YT_CHECK_EQ(yt8531c_set_loopback(&fx.dev, YT8531C_LOOPBACK_DIGITAL),
		    YT8531C_OK);
	YT_CHECK_EQ((fx.mock.mii_get(YT8531C_REG_BMCR) &
		     YT8531C_BMCR_LOOPBACK) != 0u,
		    1);
	YT_CHECK_EQ(yt8531c_set_loopback(&fx.dev, YT8531C_LOOPBACK_OFF),
		    YT8531C_OK);
	YT_CHECK_EQ((fx.mock.mii_get(YT8531C_REG_BMCR) &
		     YT8531C_BMCR_LOOPBACK) != 0u,
		    0);

	YT_CHECK_EQ(yt8531c_set_loopback(&fx.dev, YT8531C_LOOPBACK_REMOTE),
		    YT8531C_OK);
	YT_CHECK_EQ((fx.mock.ext_get(YT8531C_EXT_MISC_CFG) &
		     YT8531C_MISC_CFG_REMOTE_LOOPBACK) != 0u,
		    1);
	YT_CHECK_EQ(yt8531c_set_loopback(&fx.dev,
					 YT8531C_LOOPBACK_REMOTE_UPLOAD),
		    YT8531C_OK);
	YT_CHECK_EQ((fx.mock.ext_get(YT8531C_EXT_MISC_CFG) &
		     YT8531C_MISC_CFG_ULDATA_RLOOPBACK) != 0u,
		    1);
	YT_CHECK_EQ(yt8531c_set_loopback(&fx.dev, YT8531C_LOOPBACK_OFF),
		    YT8531C_OK);
	YT_CHECK_EQ(fx.mock.ext_get(YT8531C_EXT_MISC_CFG) & 0x0030u, 0);
	YT_CHECK_EQ(yt8531c_set_loopback(&fx.dev, 9), YT8531C_ERR_ARG);

	YT_CHECK_EQ(yt8531c_set_test_mode(&fx.dev, YT8531C_TEST_MODE_2),
		    YT8531C_OK);
	value = fx.mock.mii_get(YT8531C_REG_CTRL1000);
	YT_CHECK_EQ(field(YT8531C_CTRL1000_TEST_MASK, value), 2);
	YT_CHECK_EQ(yt8531c_set_test_mode(&fx.dev, YT8531C_TEST_MODE_NORMAL),
		    YT8531C_OK);
	YT_CHECK_EQ(fx.mock.mii_get(YT8531C_REG_CTRL1000) & 0xe000u, 0);
	YT_CHECK_EQ(yt8531c_set_test_mode(&fx.dev, 5), YT8531C_ERR_ARG);

	YT_CHECK_EQ(yt8531c_set_jumbo(&fx.dev, 1), YT8531C_OK);
	YT_CHECK_EQ((fx.mock.ext_get(YT8531C_EXT_MISC_CFG) &
		     YT8531C_MISC_CFG_JUMBO_ENABLE) != 0u,
		    1);
	YT_CHECK_EQ(yt8531c_set_jumbo(&fx.dev, 0), YT8531C_OK);
	YT_CHECK_EQ((fx.mock.ext_get(YT8531C_EXT_MISC_CFG) &
		     YT8531C_MISC_CFG_JUMBO_ENABLE) != 0u,
		    0);
	YT_CHECK_EQ(yt8531c_set_jumbo(&fx.dev, 2), YT8531C_ERR_ARG);
}

void test_power_management(void)
{
	Fixture fx;
	const yt_u8 mac[6] = { 0x02, 0xaa, 0xbb, 0xcc, 0xdd, 0xee };

	fx.mock.ext_set(YT8531C_EXT_SLEEP_CTRL, 0x8000u);
	YT_CHECK_EQ(fx.init_board(), YT8531C_OK);

	YT_CHECK_EQ(yt8531c_suspend(&fx.dev), YT8531C_OK);
	YT_CHECK_EQ((fx.mock.mii_get(YT8531C_REG_BMCR) &
		     YT8531C_BMCR_POWER_DOWN) != 0u,
		    1);

	YT_CHECK_EQ(yt8531c_resume(&fx.dev), YT8531C_OK);
	YT_CHECK_EQ((fx.mock.mii_get(YT8531C_REG_BMCR) &
		     YT8531C_BMCR_POWER_DOWN) != 0u,
		    0);
	/* Auto-sleep is a board choice: untouched when not configured off. */
	YT_CHECK_EQ((fx.mock.ext_get(YT8531C_EXT_SLEEP_CTRL) &
		     YT8531C_SLEEP_CTRL_SLEEP_SW) != 0u,
		    1);

	/* With auto-sleep disabled in the configuration, resume re-applies it. */
	fx.cfg.auto_sleep_disabled = 1;
	fx.reload();
	fx.mock.ext_set(YT8531C_EXT_SLEEP_CTRL, 0x8000u);
	YT_CHECK_EQ(yt8531c_resume(&fx.dev), YT8531C_OK);
	YT_CHECK_EQ((fx.mock.ext_get(YT8531C_EXT_SLEEP_CTRL) &
		     YT8531C_SLEEP_CTRL_SLEEP_SW) != 0u,
		    0);

	YT_CHECK_EQ(yt8531c_wol_set(&fx.dev, mac, 1, 0,
				    YT8531C_WOL_PULSE_84MS),
		    YT8531C_OK);
	YT_CHECK_EQ(yt8531c_suspend(&fx.dev), YT8531C_OK);
	YT_CHECK_EQ((fx.mock.mii_get(YT8531C_REG_BMCR) &
		     YT8531C_BMCR_POWER_DOWN) != 0u,
		    0);
	YT_CHECK_EQ(yt8531c_resume(&fx.dev), YT8531C_OK);
}
