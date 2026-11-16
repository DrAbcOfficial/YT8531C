/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef YT8531C_TESTS_HPP
#define YT8531C_TESTS_HPP

void test_config_defaults(void);
void test_init_validation(void);
void test_probe_identification(void);
void test_reset(void);
void test_config_init_rgmii(void);
void test_config_init_plain_rgmii(void);
void test_config_init_txid(void);
void test_config_init_pad_drive(void);
void test_config_init_clock_output(void);
void test_config_init_misc(void);
void test_config_init_wol(void);
void test_drive_strength_mapping(void);
void test_dump(void);
void test_aneg(void);
void test_force_and_status(void);
void test_features_and_counters(void);
void test_led(void);
void test_wol(void);
void test_interrupts(void);
void test_eee(void);
void test_loopback_test_mode(void);
void test_power_management(void);

#endif /* YT8531C_TESTS_HPP */
