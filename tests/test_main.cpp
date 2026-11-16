/* SPDX-License-Identifier: GPL-3.0-only */

#include "test_framework.hpp"
#include "tests.hpp"
#include "yt8531c/yt8531c.h"

int main(void)
{
	printf("YT8531C core unit tests (version 0x%08x)\n",
	       (unsigned int)yt8531c_version());

	yt_test_run("config_defaults", test_config_defaults);
	yt_test_run("init_validation", test_init_validation);
	yt_test_run("probe_identification", test_probe_identification);
	yt_test_run("reset", test_reset);
	yt_test_run("config_init_rgmii", test_config_init_rgmii);
	yt_test_run("config_init_plain_rgmii", test_config_init_plain_rgmii);
	yt_test_run("config_init_txid", test_config_init_txid);
	yt_test_run("config_init_pad_drive", test_config_init_pad_drive);
	yt_test_run("config_init_clock_output", test_config_init_clock_output);
	yt_test_run("config_init_misc", test_config_init_misc);
	yt_test_run("config_init_wol", test_config_init_wol);
	yt_test_run("dump", test_dump);
	yt_test_run("aneg", test_aneg);
	yt_test_run("force_and_status", test_force_and_status);
	yt_test_run("features_and_counters", test_features_and_counters);
	yt_test_run("led", test_led);
	yt_test_run("wol", test_wol);
	yt_test_run("interrupts", test_interrupts);
	yt_test_run("eee", test_eee);
	yt_test_run("loopback_test_mode", test_loopback_test_mode);
	yt_test_run("power_management", test_power_management);

	printf("\n%d checks, %d failure(s)\n", yt_test_checks(),
	       yt_test_failures());
	return (yt_test_failures() == 0) ? 0 : 1;
}
