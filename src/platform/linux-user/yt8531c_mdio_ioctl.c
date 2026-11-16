/* SPDX-License-Identifier: GPL-3.0-only */

#define _GNU_SOURCE

#include "yt8531c_mdio_ioctl.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static void fill_ifname(struct ifreq *ifr, const char *ifname)
{
	size_t len = strlen(ifname);

	if (len >= IF_NAMESIZE)
		len = IF_NAMESIZE - 1u;
	memcpy(ifr->ifr_name, ifname, len);
	ifr->ifr_name[len] = 0;
}

static int mdio_xfer(struct yt8531c_mdio_ctx *ctx, unsigned long request,
		     yt_u8 reg, yt_u16 *value)
{
	struct ifreq ifr;
	struct yt8531c_mii_ioctl mii;
	int rc;

	memset(&ifr, 0, sizeof(ifr));
	fill_ifname(&ifr, ctx->ifname);

	memset(&mii, 0, sizeof(mii));
	mii.phy_id = (yt_u16)ctx->phy_addr;
	mii.reg_num = reg;
	mii.val_in = (value != NULL) ? *value : 0;
	ifr.ifr_data = (char *)&mii;

	rc = ioctl(ctx->fd, request, &ifr);
	if (rc < 0) {
		if (ctx->verbose)
			fprintf(stderr, "[yt8531c] %s reg 0x%02x: %s\n",
				request == SIOCGMIIREG ? "read" : "write", reg,
				strerror(errno));
		return -1;
	}

	if (value != NULL)
		*value = mii.val_out;
	return 0;
}

static int port_read(void *port_ctx, yt_u8 phy_addr, yt_u8 reg, yt_u16 *value)
{
	struct yt8531c_mdio_ctx *ctx = port_ctx;

	(void)phy_addr;
	return mdio_xfer(ctx, SIOCGMIIREG, reg, value);
}

static int port_write(void *port_ctx, yt_u8 phy_addr, yt_u8 reg, yt_u16 value)
{
	struct yt8531c_mdio_ctx *ctx = port_ctx;

	(void)phy_addr;
	return mdio_xfer(ctx, SIOCSMIIREG, reg, &value);
}

static void port_delay(void *port_ctx, yt_u32 us)
{
	(void)port_ctx;

	struct timespec ts;

	ts.tv_sec = (time_t)(us / 1000000u);
	ts.tv_nsec = (long)(us % 1000000u) * 1000L;
	while (nanosleep(&ts, &ts) < 0 && errno == EINTR)
		;
}

static void port_log(void *port_ctx, yt_u8 level, const char *msg, yt_u32 len)
{
	struct yt8531c_mdio_ctx *ctx = port_ctx;
	static const char *const names[] = { "debug", "info", "warn", "error" };

	if (!ctx->verbose)
		return;

	fprintf(stderr, "[yt8531c %s] %.*s\n",
		names[level < 4u ? level : 3u], (int)len, msg);
}

int yt8531c_mdio_open(struct yt8531c_mdio_ctx *ctx, const char *ifname,
		      int phy_addr)
{
	struct ifreq ifr;
	struct yt8531c_mii_ioctl mii;

	if (ctx == NULL || ifname == NULL)
		return -1;
	if (strlen(ifname) >= IF_NAMESIZE)
		return -1;

	memset(ctx, 0, sizeof(*ctx));
	ctx->fd = -1;
	memcpy(ctx->ifname, ifname, strlen(ifname) + 1u);

	ctx->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (ctx->fd < 0)
		return -1;

	if (phy_addr >= 0) {
		ctx->phy_addr = (unsigned int)phy_addr;
		return 0;
	}

	memset(&ifr, 0, sizeof(ifr));
	memset(&mii, 0, sizeof(mii));
	fill_ifname(&ifr, ctx->ifname);
	ifr.ifr_data = (char *)&mii;

	if (ioctl(ctx->fd, SIOCGMIIPHY, &ifr) < 0) {
		int saved = errno;

		close(ctx->fd);
		ctx->fd = -1;
		errno = saved;
		return -1;
	}

	ctx->phy_addr = mii.phy_id;
	return 0;
}

void yt8531c_mdio_close(struct yt8531c_mdio_ctx *ctx)
{
	if (ctx != NULL && ctx->fd >= 0) {
		close(ctx->fd);
		ctx->fd = -1;
	}
}

yt8531c_port yt8531c_mdio_port(struct yt8531c_mdio_ctx *ctx)
{
	yt8531c_port port = YT8531C_PORT_INITIALIZER;

	port.ctx = ctx;
	port.mdio_read = port_read;
	port.mdio_write = port_write;
	port.delay_us = port_delay;
	port.log = port_log;
	port.caps = YT8531C_PORT_CAP_LOG;
	return port;
}
