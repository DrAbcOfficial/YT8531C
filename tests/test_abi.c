/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * C ABI smoke test: proves the public headers are valid C and that a C
 * consumer can drive the C++ implementation without any C++ runtime.
 */

#include "yt8531c/yt8531c.h"

#include <stdio.h>
#include <string.h>

static int abi_read(void *ctx, yt_u8 phy, yt_u8 reg, yt_u16 *value)
{
	(void)ctx;
	(void)phy;

	if (reg == YT8531C_REG_PHYSID1) {
		*value = 0x4f51u;
		return 0;
	}
	if (reg == YT8531C_REG_PHYSID2) {
		*value = 0xe91bu;
		return 0;
	}
	if (reg == YT8531C_REG_EXT_DATA) {
		*value = 0x0140u;
		return 0;
	}
	*value = 0u;
	return 0;
}

static int abi_write(void *ctx, yt_u8 phy, yt_u8 reg, yt_u16 value)
{
	(void)ctx;
	(void)phy;
	(void)reg;
	(void)value;
	return 0;
}

static void abi_delay(void *ctx, yt_u32 us)
{
	(void)ctx;
	(void)us;
}

int main(void)
{
	yt8531c_config cfg;
	yt8531c_port port = YT8531C_PORT_INITIALIZER;
	yt8531c_dev dev;
	yt8531c_id id;
	int failures = 0;

	if (yt8531c_version() != YT8531C_VERSION) {
		printf("version mismatch\n");
		++failures;
	}

	if (yt8531c_context_size() != (yt_u32)sizeof(dev)) {
		printf("context size mismatch\n");
		++failures;
	}

	yt8531c_config_default(&cfg);
	if (cfg.struct_size != (yt_u32)sizeof(cfg)) {
		printf("struct size mismatch\n");
		++failures;
	}

	port.ctx = NULL;
	port.mdio_read = abi_read;
	port.mdio_write = abi_write;
	port.delay_us = abi_delay;

	if (yt8531c_init(&dev, &cfg, &port) != YT8531C_OK) {
		printf("init failed\n");
		++failures;
	}

	memset(&id, 0, sizeof(id));
	if (yt8531c_probe(&dev, &id) != YT8531C_OK) {
		printf("probe failed\n");
		++failures;
	}
	if (id.id != YT8531C_PHY_ID_YT8531) {
		printf("unexpected id 0x%08x\n", (unsigned int)id.id);
		++failures;
	}

	if (strcmp(yt8531c_strerror(YT8531C_ERR_IO), "MDIO transfer failed") != 0) {
		printf("strerror mismatch\n");
		++failures;
	}

	printf("C ABI test: %s\n", failures == 0 ? "ok" : "FAILED");
	return (failures == 0) ? 0 : 1;
}
