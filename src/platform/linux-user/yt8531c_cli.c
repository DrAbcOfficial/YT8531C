/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * yt8531cctl - Linux userspace control tool for the YT8531C PHY.
 *
 * The tool talks to the PHY through the standard SIOCGMIIREG/SIOCSMIIREG
 * ioctls of the netdev that hosts it, so it works with any MAC driver that
 * implements the generic MII ioctl handlers.  Root (CAP_NET_ADMIN) is needed.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yt8531c/yt8531c.h"
#include "yt8531c_mdio_ioctl.h"

struct options {
	const char *ifname;
	int phy_addr;
	int verbose;
	int interface;
	unsigned int tx_delay_ps;
	unsigned int rx_delay_ps;
	int have_tx_delay;
	int have_rx_delay;
};

static void usage(FILE *out)
{
	fprintf(out,
		"usage: yt8531cctl -i IFACE [-p PHY] [-m MODE] [-t PS] [-r PS] [-v] COMMAND [ARGS]\n"
		"\n"
		"  -i IFACE   network interface whose MDIO bus hosts the PHY\n"
		"  -p PHY     PHY address (default: query with SIOCGMIIPHY)\n"
		"  -m MODE    RGMII mode for init/aneg: rgmii, rxid, txid, id\n"
		"  -t PS      TX internal delay in picoseconds (0..2250, step 150)\n"
		"  -r PS      RX internal delay in picoseconds (0..2250, step 150)\n"
		"  -v         verbose (log core diagnostics to stderr)\n"
		"\n"
		"commands:\n"
		"  id                          identify the PHY\n"
		"  status                      link status and negotiated parameters\n"
		"  features                    static capabilities\n"
		"  init                        apply configuration and start auto-negotiation\n"
		"  aneg                        restart auto-negotiation\n"
		"  force SPEED [full|half]     force speed (10/100/1000), or 0 for auto\n"
		"  reset                       software reset\n"
		"  dump                        register snapshot\n"
		"  mii-read REG                read clause 22 register (hex)\n"
		"  mii-write REG VAL           write clause 22 register (hex)\n"
		"  ext-read ADDR               read extended register (hex)\n"
		"  ext-write ADDR VAL          write extended register (hex)\n"
		"  mmd-read DEVAD REG          read MMD register\n"
		"  mmd-write DEVAD REG VAL     write MMD register\n"
		"  led N TRIGGERS              set LED triggers (link,10,100,1000,fd,hd,tx,rx,act,col)\n"
		"  led-read N                  read LED configuration\n"
		"  led-off N                   disable LED triggers\n"
		"  blink DUTY FREQ1 FREQ2      blink timing (duty 0..7, freq 0..3)\n"
		"  wol MAC                     enable Wake-on-LAN magic packet\n"
		"  wol-off                     disable Wake-on-LAN\n"
		"  wol-status                  show Wake-on-LAN configuration\n"
		"  eee on|off                  enable or disable EEE advertisement\n"
		"  eee-status                  show EEE state\n"
		"  loopback off|digital|remote|upload\n"
		"  test MODE                   test mode 0..4 (0 = normal)\n"
		"  jumbo on|off                jumbo frame support\n"
		"  counters                    error counters\n");
}

static int parse_uint(const char *text, unsigned int base, unsigned int max,
		      unsigned int *value)
{
	char *end = NULL;
	unsigned long parsed;

	if (text == NULL || *text == '\0')
		return -1;

	errno = 0;
	parsed = strtoul(text, &end, (int)base);
	if (errno != 0 || end == text || *end != '\0' || parsed > max)
		return -1;

	*value = (unsigned int)parsed;
	return 0;
}

static int parse_mac(const char *text, yt_u8 mac[6])
{
	unsigned int values[6];
	int count;

	count = sscanf(text, "%x:%x:%x:%x:%x:%x", &values[0], &values[1],
		       &values[2], &values[3], &values[4], &values[5]);
	if (count != 6)
		return -1;
	for (int i = 0; i < 6; ++i) {
		if (values[i] > 0xffu)
			return -1;
		mac[i] = (yt_u8)values[i];
	}
	return 0;
}

static int parse_interface(const char *text, int *value)
{
	if (strcmp(text, "rgmii") == 0)
		*value = YT8531C_IF_RGMII;
	else if (strcmp(text, "rxid") == 0)
		*value = YT8531C_IF_RGMII_RXID;
	else if (strcmp(text, "txid") == 0)
		*value = YT8531C_IF_RGMII_TXID;
	else if (strcmp(text, "id") == 0)
		*value = YT8531C_IF_RGMII_ID;
	else
		return -1;
	return 0;
}

static const char *speed_name(yt_u16 speed)
{
	switch (speed) {
	case YT8531C_SPEED_10:
		return "10";
	case YT8531C_SPEED_100:
		return "100";
	case YT8531C_SPEED_1000:
		return "1000";
	default:
		return "unknown";
	}
}

static const char *duplex_name(yt_u16 duplex)
{
	switch (duplex) {
	case YT8531C_DUPLEX_FULL:
		return "full";
	case YT8531C_DUPLEX_HALF:
		return "half";
	default:
		return "unknown";
	}
}

static int require_rc(yt_rc rc, const char *what)
{
	if (rc == YT8531C_OK)
		return 0;

	fprintf(stderr, "%s: %s\n", what, yt8531c_strerror(rc));
	return 1;
}

static int cmd_id(yt8531c_dev *dev)
{
	yt8531c_id id;
	yt_rc rc = yt8531c_probe(dev, &id);

	if (rc != YT8531C_OK)
		return require_rc(rc, "probe");

	printf("phy id      : 0x%08x\n", (unsigned int)id.id);
	printf("id1/id2     : 0x%04x / 0x%04x\n", id.id1, id.id2);
	printf("model       : 0x%02x\n", id.model);
	printf("revision    : 0x%02x\n", id.revision);
	printf("chip config : 0x%04x\n", id.chip_config);
	printf("ldo voltage : %s\n",
	       id.ldo_voltage == YT8531C_LDO_VOLTAGE_3V3   ? "3.3V"
	       : id.ldo_voltage == YT8531C_LDO_VOLTAGE_2V5 ? "2.5V"
							  : "1.8V");
	printf("supported   : %s\n", id.supported ? "yes" : "no");
	return 0;
}

static int cmd_status(yt8531c_dev *dev)
{
	yt8531c_status status;
	yt8531c_features features;
	yt_rc rc;

	rc = yt8531c_read_status(dev, &status);
	if (rc != YT8531C_OK)
		return require_rc(rc, "read status");

	rc = yt8531c_get_features(dev, &features);
	if (rc != YT8531C_OK)
		return require_rc(rc, "read features");

	printf("link        : %s\n", status.link ? "up" : "down");
	printf("speed       : %s Mbit/s\n", speed_name(status.speed));
	printf("duplex      : %s\n", duplex_name(status.duplex));
	printf("autoneg     : %s\n",
	       status.autoneg_complete ? "complete" : "in progress");
	printf("resolved    : %s\n", status.resolved ? "yes" : "no");
	printf("mdix        : %s\n", status.mdix == 2 ? "unknown"
				   : status.mdix    ? "MDIX"
						    : "MDI");
	printf("pause       : tx=%u rx=%u\n", status.tx_pause,
	       status.rx_pause);
	printf("polarity    : %s\n",
	       status.polarity_inverted ? "inverted" : "normal");
	printf("master/slave: %s\n",
	       status.master_slave == YT8531C_MS_MASTER  ? "master"
	       : status.master_slave == YT8531C_MS_SLAVE ? "slave"
							 : "unknown");
	printf("lpa         : 0x%04x\n", status.lpa);
	printf("stat1000    : 0x%04x\n", status.stat1000);
	printf("features    : 0x%04x modes, 0x%04x flags\n", features.modes,
	       features.flags);
	return 0;
}

static int cmd_init(yt8531c_dev *dev)
{
	yt_rc rc;

	rc = yt8531c_config_init(dev);
	if (rc != YT8531C_OK)
		return require_rc(rc, "configure");

	rc = yt8531c_config_aneg(dev);
	if (rc != YT8531C_OK)
		return require_rc(rc, "start auto-negotiation");

	printf("PHY configured, auto-negotiation restarted\n");
	return 0;
}

static int cmd_dump(yt8531c_dev *dev)
{
	yt8531c_snapshot dump;
	yt_rc rc = yt8531c_dump(dev, &dump);

	if (rc != YT8531C_OK)
		return require_rc(rc, "dump");

	printf("clause 22 registers:\n");
	for (unsigned int i = 0; i < YT8531C_DUMP_MII_COUNT; ++i) {
		printf("  [0x%02x] 0x%04x%s", i, dump.mii[i],
		       ((i % 4u) == 3u) ? "\n" : "");
	}

	printf("extended registers:\n");
	for (unsigned int i = 0; i < dump.ext_count; ++i)
		printf("  [0x%04x] 0x%04x%s", dump.ext[i].addr,
		       dump.ext[i].value, ((i % 3u) == 2u) ? "\n" : "");
	if ((dump.ext_count % 3u) != 0u)
		printf("\n");

	printf("MMD registers:\n");
	for (unsigned int i = 0; i < dump.mmd_count; ++i)
		printf("  [devad %u, 0x%04x] 0x%04x\n", dump.mmd[i].devad,
		       dump.mmd[i].reg, dump.mmd[i].value);
	return 0;
}

static int led_trigger_bit(const char *name, yt_u16 *bit)
{
	static const struct {
		const char *name;
		yt_u16 bit;
	} map[] = {
		{ "link", YT8531C_LED_TRIG_LINK },
		{ "10", YT8531C_LED_TRIG_LINK_10 },
		{ "100", YT8531C_LED_TRIG_LINK_100 },
		{ "1000", YT8531C_LED_TRIG_LINK_1000 },
		{ "fd", YT8531C_LED_TRIG_FULL_DUPLEX },
		{ "hd", YT8531C_LED_TRIG_HALF_DUPLEX },
		{ "tx", YT8531C_LED_TRIG_TX },
		{ "rx", YT8531C_LED_TRIG_RX },
		{ "act", YT8531C_LED_TRIG_ACTIVITY },
		{ "col", YT8531C_LED_TRIG_COLLISION },
	};

	for (unsigned int i = 0; i < YT8531C_ARRAY_SIZE(map); ++i) {
		if (strcmp(name, map[i].name) == 0) {
			*bit = map[i].bit;
			return 0;
		}
	}
	return -1;
}

static int parse_triggers(const char *text, yt_u16 *triggers)
{
	char buffer[128];
	char *save = NULL;
	char *token;
	yt_u16 result = 0;

	if (strlen(text) >= sizeof(buffer))
		return -1;
	strcpy(buffer, text);

	for (token = strtok_r(buffer, ",", &save); token != NULL;
	     token = strtok_r(NULL, ",", &save)) {
		yt_u16 bit;

		if (led_trigger_bit(token, &bit) < 0)
			return -1;
		result = (yt_u16)(result | bit);
	}

	*triggers = result;
	return 0;
}

static int led_read_and_print(yt8531c_dev *dev, unsigned int index)
{
	yt8531c_led_cfg cfg;
	yt_rc rc = yt8531c_led_get(dev, (yt_u8)index, &cfg);

	if (rc != YT8531C_OK)
		return require_rc(rc, "read LED");

	printf("led %u: triggers=0x%04x force=%s\n", index, cfg.triggers,
	       cfg.force_mode == YT8531C_LED_FORCE_AUTO ? "auto"
	       : cfg.force_mode == YT8531C_LED_FORCE_OFF   ? "off"
	       : cfg.force_mode == YT8531C_LED_FORCE_ON    ? "on"
							   : "blink");
	printf("        blink duty=%u freq1=%u freq2=%u\n", cfg.blink_duty,
	       cfg.blink_freq1, cfg.blink_freq2);
	return 0;
}

static int run_command(yt8531c_dev *dev, int argc, char **argv)
{
	const char *cmd = argv[0];
	unsigned int value = 0;
	unsigned int value2 = 0;
	yt_rc rc;

	if (strcmp(cmd, "id") == 0)
		return cmd_id(dev);
	if (strcmp(cmd, "status") == 0)
		return cmd_status(dev);
	if (strcmp(cmd, "features") == 0) {
		yt8531c_features features;

		rc = yt8531c_get_features(dev, &features);
		if (rc != YT8531C_OK)
			return require_rc(rc, "features");
		printf("modes: 0x%04x\nflags: 0x%04x\n", features.modes,
		       features.flags);
		return 0;
	}
	if (strcmp(cmd, "init") == 0)
		return cmd_init(dev);
	if (strcmp(cmd, "aneg") == 0) {
		rc = yt8531c_config_aneg(dev);
		if (rc != YT8531C_OK)
			return require_rc(rc, "aneg");
		printf("auto-negotiation restarted\n");
		return 0;
	}
	if (strcmp(cmd, "reset") == 0) {
		rc = yt8531c_reset(dev);
		if (rc != YT8531C_OK)
			return require_rc(rc, "reset");
		printf("PHY reset\n");
		return 0;
	}
	if (strcmp(cmd, "dump") == 0)
		return cmd_dump(dev);

	if (strcmp(cmd, "force") == 0) {
		unsigned int speed;
		int full = 1;

		if (argc < 2 || parse_uint(argv[1], 10, 1000, &speed) < 0) {
			fprintf(stderr, "force: invalid speed\n");
			return 1;
		}
		if (argc >= 3)
			full = (strcmp(argv[2], "full") == 0);
		rc = yt8531c_force_link(
			dev, speed == 0u ? YT8531C_SPEED_UNKNOWN : (yt_u16)speed,
			full ? YT8531C_DUPLEX_FULL : YT8531C_DUPLEX_HALF);
		return require_rc(rc, "force");
	}

	if (strcmp(cmd, "mii-read") == 0 || strcmp(cmd, "mii-write") == 0) {
		yt_u16 reg;
		yt_u16 val = 0;

		if (argc < 2 || parse_uint(argv[1], 16, 0x1f, &value) < 0) {
			fprintf(stderr, "%s: invalid register\n", cmd);
			return 1;
		}
		reg = (yt_u16)value;
		if (cmd[4] == 'w') {
			if (argc < 3 || parse_uint(argv[2], 16, 0xffff, &value2) < 0) {
				fprintf(stderr, "mii-write: invalid value\n");
				return 1;
			}
			return require_rc(yt8531c_mii_write(dev, (yt_u8)reg,
							   (yt_u16)value2),
					  "mii-write");
		}
		rc = yt8531c_mii_read(dev, (yt_u8)reg, &val);
		if (rc != YT8531C_OK)
			return require_rc(rc, "mii-read");
		printf("0x%02x = 0x%04x\n", reg, val);
		return 0;
	}

	if (strcmp(cmd, "ext-read") == 0 || strcmp(cmd, "ext-write") == 0) {
		yt_u16 reg;
		yt_u16 val = 0;

		if (argc < 2 || parse_uint(argv[1], 16, 0xffff, &value) < 0) {
			fprintf(stderr, "%s: invalid address\n", cmd);
			return 1;
		}
		reg = (yt_u16)value;
		if (cmd[4] == 'w') {
			if (argc < 3 || parse_uint(argv[2], 16, 0xffff, &value2) < 0) {
				fprintf(stderr, "ext-write: invalid value\n");
				return 1;
			}
			return require_rc(yt8531c_ext_write(dev, reg,
							    (yt_u16)value2),
					  "ext-write");
		}
		rc = yt8531c_ext_read(dev, reg, &val);
		if (rc != YT8531C_OK)
			return require_rc(rc, "ext-read");
		printf("0x%04x = 0x%04x\n", reg, val);
		return 0;
	}

	if (strcmp(cmd, "mmd-read") == 0 || strcmp(cmd, "mmd-write") == 0) {
		unsigned int devad;
		unsigned int reg;
		yt_u16 val = 0;

		if (argc < 3 || parse_uint(argv[1], 10, 31, &devad) < 0 ||
		    parse_uint(argv[2], 16, 0xffff, &reg) < 0) {
			fprintf(stderr, "%s: invalid devad/register\n", cmd);
			return 1;
		}
		if (cmd[4] == 'w') {
			if (argc < 4 || parse_uint(argv[3], 16, 0xffff, &value2) < 0) {
				fprintf(stderr, "mmd-write: invalid value\n");
				return 1;
			}
			return require_rc(yt8531c_mmd_write(dev, (yt_u8)devad,
							    (yt_u16)reg,
							    (yt_u16)value2),
					  "mmd-write");
		}
		rc = yt8531c_mmd_read(dev, (yt_u8)devad, (yt_u16)reg, &val);
		if (rc != YT8531C_OK)
			return require_rc(rc, "mmd-read");
		printf("devad %u [0x%04x] = 0x%04x\n", devad, reg, val);
		return 0;
	}

	if (strcmp(cmd, "led") == 0) {
		yt8531c_led_cfg cfg;
		yt_u16 triggers;

		if (argc < 3 || parse_uint(argv[1], 10, 2, &value) < 0 ||
		    parse_triggers(argv[2], &triggers) < 0) {
			fprintf(stderr, "led: invalid index or triggers\n");
			return 1;
		}
		memset(&cfg, 0, sizeof(cfg));
		cfg.triggers = triggers;
		cfg.force_mode = YT8531C_LED_FORCE_AUTO;
		cfg.blink_duty = YT8531C_AUTO;
		cfg.blink_freq1 = YT8531C_AUTO;
		cfg.blink_freq2 = YT8531C_AUTO;
		rc = yt8531c_led_set(dev, (yt_u8)value, &cfg);
		if (rc != YT8531C_OK)
			return require_rc(rc, "led");
		return led_read_and_print(dev, value);
	}
	if (strcmp(cmd, "led-off") == 0) {
		yt8531c_led_cfg cfg;

		if (argc < 2 || parse_uint(argv[1], 10, 2, &value) < 0) {
			fprintf(stderr, "led-off: invalid index\n");
			return 1;
		}
		memset(&cfg, 0, sizeof(cfg));
		cfg.triggers = 0;
		cfg.force_mode = YT8531C_LED_FORCE_AUTO;
		cfg.blink_duty = YT8531C_AUTO;
		cfg.blink_freq1 = YT8531C_AUTO;
		cfg.blink_freq2 = YT8531C_AUTO;
		rc = yt8531c_led_set(dev, (yt_u8)value, &cfg);
		return require_rc(rc, "led-off");
	}
	if (strcmp(cmd, "led-read") == 0) {
		if (argc < 2 || parse_uint(argv[1], 10, 2, &value) < 0) {
			fprintf(stderr, "led-read: invalid index\n");
			return 1;
		}
		return led_read_and_print(dev, value);
	}
	if (strcmp(cmd, "blink") == 0) {
		unsigned int duty;
		unsigned int freq1;
		unsigned int freq2;

		if (argc < 4 || parse_uint(argv[1], 10, 7, &duty) < 0 ||
		    parse_uint(argv[2], 10, 3, &freq1) < 0 ||
		    parse_uint(argv[3], 10, 3, &freq2) < 0) {
			fprintf(stderr, "blink: invalid duty/frequency\n");
			return 1;
		}
		rc = yt8531c_led_blink_set(dev, (yt_u8)duty, (yt_u8)freq1,
					   (yt_u8)freq2);
		return require_rc(rc, "blink");
	}

	if (strcmp(cmd, "wol") == 0) {
		yt_u8 mac[6];

		if (argc < 2 || parse_mac(argv[1], mac) < 0) {
			fprintf(stderr, "wol: invalid MAC address\n");
			return 1;
		}
		rc = yt8531c_wol_set(dev, mac, 1, 0, YT8531C_WOL_PULSE_168MS);
		if (rc != YT8531C_OK)
			return require_rc(rc, "wol");
		printf("Wake-on-LAN enabled\n");
		return 0;
	}
	if (strcmp(cmd, "wol-off") == 0)
		return require_rc(yt8531c_wol_set(dev, NULL, 0, 0, 0),
				  "wol-off");
	if (strcmp(cmd, "wol-status") == 0) {
		yt_u8 mac[6] = { 0 };
		yt_u8 enable = 0;
		yt_u8 pmeb = 0;
		yt_u8 pulse = 0;

		rc = yt8531c_wol_get(dev, mac, &enable, &pmeb, &pulse);
		if (rc != YT8531C_OK)
			return require_rc(rc, "wol-status");
		printf("wol    : %s\n", enable ? "enabled" : "disabled");
		printf("pmeb   : %s\n", pmeb ? "PME_N" : "INT_N");
		printf("pulse  : %u\n", pulse);
		printf("mac    : %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0],
		       mac[1], mac[2], mac[3], mac[4], mac[5]);
		return 0;
	}

	if (strcmp(cmd, "eee") == 0) {
		yt_u16 ability;

		if (argc < 2) {
			fprintf(stderr, "eee: expected on or off\n");
			return 1;
		}
		if (strcmp(argv[1], "on") == 0)
			ability = (yt_u16)(YT8531C_EEE_ABILITY_1000BASE_T |
					   YT8531C_EEE_ABILITY_100BASE_TX);
		else if (strcmp(argv[1], "off") == 0)
			ability = 0;
		else {
			fprintf(stderr, "eee: expected on or off\n");
			return 1;
		}
		rc = yt8531c_eee_set(dev, ability);
		return require_rc(rc, "eee");
	}
	if (strcmp(cmd, "eee-status") == 0) {
		yt8531c_eee eee;

		rc = yt8531c_eee_get(dev, &eee);
		if (rc != YT8531C_OK)
			return require_rc(rc, "eee-status");
		printf("capabilities : 0x%04x\n", eee.capabilities);
		printf("local ability: 0x%04x\n", eee.local_ability);
		printf("lp ability   : 0x%04x\n", eee.lp_ability);
		printf("pcs status   : 0x%04x\n", eee.pcs_status);
		printf("wake errors  : %u\n", eee.wake_errors);
		return 0;
	}

	if (strcmp(cmd, "loopback") == 0) {
		yt_u8 mode;

		if (argc < 2)
			return 1;
		if (strcmp(argv[1], "off") == 0)
			mode = YT8531C_LOOPBACK_OFF;
		else if (strcmp(argv[1], "digital") == 0)
			mode = YT8531C_LOOPBACK_DIGITAL;
		else if (strcmp(argv[1], "remote") == 0)
			mode = YT8531C_LOOPBACK_REMOTE;
		else if (strcmp(argv[1], "upload") == 0)
			mode = YT8531C_LOOPBACK_REMOTE_UPLOAD;
		else {
			fprintf(stderr, "loopback: invalid mode\n");
			return 1;
		}
		return require_rc(yt8531c_set_loopback(dev, mode), "loopback");
	}
	if (strcmp(cmd, "test") == 0) {
		if (argc < 2 || parse_uint(argv[1], 10, 4, &value) < 0) {
			fprintf(stderr, "test: invalid mode\n");
			return 1;
		}
		return require_rc(yt8531c_set_test_mode(dev, (yt_u8)value),
				  "test");
	}
	if (strcmp(cmd, "jumbo") == 0) {
		if (argc < 2)
			return 1;
		if (strcmp(argv[1], "on") != 0 && strcmp(argv[1], "off") != 0) {
			fprintf(stderr, "jumbo: expected on or off\n");
			return 1;
		}
		return require_rc(
			yt8531c_set_jumbo(dev, strcmp(argv[1], "on") == 0),
			"jumbo");
	}
	if (strcmp(cmd, "counters") == 0) {
		yt8531c_counters counters;

		rc = yt8531c_get_counters(dev, &counters);
		if (rc != YT8531C_OK)
			return require_rc(rc, "counters");
		printf("rx errors      : %u\n", counters.rx_errors);
		printf("idle errors    : %u\n", counters.idle_errors);
		printf("eee wake errors: %u\n", counters.eee_wake_errors);
		return 0;
	}

	if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0) {
		usage(stdout);
		return 0;
	}

	fprintf(stderr, "unknown command: %s\n", cmd);
	usage(stderr);
	return 1;
}

int main(int argc, char **argv)
{
	struct options opts = {
		.ifname = NULL,
		.phy_addr = -1,
		.verbose = 0,
		.interface = YT8531C_IF_RGMII,
		.tx_delay_ps = 0,
		.rx_delay_ps = 0,
		.have_tx_delay = 0,
		.have_rx_delay = 0,
	};
	struct yt8531c_mdio_ctx mdio;
	yt8531c_config cfg;
	yt8531c_port port;
	yt8531c_dev dev;
	int opt;

	while ((opt = getopt(argc, argv, "i:p:m:t:r:vh")) != -1) {
		switch (opt) {
		case 'i':
			opts.ifname = optarg;
			break;
		case 'p':
			if (parse_uint(optarg, 10, 31,
				       (unsigned int *)&opts.phy_addr) < 0) {
				fprintf(stderr, "invalid PHY address\n");
				return 1;
			}
			break;
		case 'm':
			if (parse_interface(optarg, &opts.interface) < 0) {
				fprintf(stderr, "invalid interface mode\n");
				return 1;
			}
			break;
		case 't':
			if (parse_uint(optarg, 10, 2250, &opts.tx_delay_ps) < 0) {
				fprintf(stderr, "invalid TX delay\n");
				return 1;
			}
			opts.have_tx_delay = 1;
			break;
		case 'r':
			if (parse_uint(optarg, 10, 2250, &opts.rx_delay_ps) < 0) {
				fprintf(stderr, "invalid RX delay\n");
				return 1;
			}
			opts.have_rx_delay = 1;
			break;
		case 'v':
			opts.verbose = 1;
			break;
		case 'h':
			usage(stdout);
			return 0;
		default:
			usage(stderr);
			return 1;
		}
	}

	if (opts.ifname == NULL || optind >= argc) {
		usage(stderr);
		return 1;
	}

	if (yt8531c_mdio_open(&mdio, opts.ifname, opts.phy_addr) < 0) {
		fprintf(stderr, "cannot open MDIO access on %s: %s\n",
			opts.ifname, strerror(errno));
		return 1;
	}
	mdio.verbose = opts.verbose;

	yt8531c_config_default(&cfg);
	cfg.phy_addr = (yt_u8)mdio.phy_addr;
	cfg.interface = (yt_u8)opts.interface;
	if (opts.have_tx_delay)
		cfg.tx_delay_ps = (yt_u16)opts.tx_delay_ps;
	if (opts.have_rx_delay)
		cfg.rx_delay_ps = (yt_u16)opts.rx_delay_ps;

	port = yt8531c_mdio_port(&mdio);
	if (yt8531c_init(&dev, &cfg, &port) != YT8531C_OK) {
		fprintf(stderr, "invalid PHY configuration\n");
		yt8531c_mdio_close(&mdio);
		return 1;
	}

	int ret = run_command(&dev, argc - optind, &argv[optind]);

	yt8531c_mdio_close(&mdio);
	return ret;
}
