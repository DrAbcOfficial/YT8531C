/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Linux userspace MDIO access through the SIOCGMIIREG/SIOCSMIIREG ioctls.
 *
 * These ioctls are implemented by most Ethernet drivers and give unprivileged
 * root access to the clause 22 register space of a PHY behind a netdev.  They
 * are enough to run the full YT8531C driver core from userspace for board
 * bring-up and diagnostics.
 */

#ifndef YT8531C_MDIO_IOCTL_H
#define YT8531C_MDIO_IOCTL_H

#include <net/if.h>

#include "yt8531c/yt8531c.h"

/*
 * The MII ioctl numbers and payload layout are part of the stable Linux UAPI.
 * They are declared here instead of pulling in <linux/mii.h>, so the tool can
 * be built against any libc (including musl) without kernel UAPI headers.
 */
#ifndef SIOCGMIIPHY
#define SIOCGMIIPHY 0x8947
#endif
#ifndef SIOCGMIIREG
#define SIOCGMIIREG 0x8948
#endif
#ifndef SIOCSMIIREG
#define SIOCSMIIREG 0x8949
#endif

struct yt8531c_mii_ioctl {
	yt_u16 phy_id;
	yt_u16 reg_num;
	yt_u16 val_in;
	yt_u16 val_out;
};

struct yt8531c_mdio_ctx {
	int fd;
	char ifname[IF_NAMESIZE];
	unsigned int phy_addr;
	int verbose;
};

/*
 * Open a datagram socket bound to @ifname.  When @phy_addr is negative the
 * PHY address is queried with SIOCGMIIPHY.  Returns 0 on success.
 */
int yt8531c_mdio_open(struct yt8531c_mdio_ctx *ctx, const char *ifname,
		      int phy_addr);

void yt8531c_mdio_close(struct yt8531c_mdio_ctx *ctx);

/* Port implementation forwarding to the ioctl backend. */
yt8531c_port yt8531c_mdio_port(struct yt8531c_mdio_ctx *ctx);

#endif /* YT8531C_MDIO_IOCTL_H */
