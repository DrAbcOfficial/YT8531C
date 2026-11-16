/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * YT8531C core: Energy Efficient Ethernet.
 */

#include "yt8531c/yt8531c.h"

#include "yt8531c_internal.hpp"

#define YT8531C_EEE_ABILITY_MASK \
	(YT8531C_EEE_ABILITY_1000BASE_T | YT8531C_EEE_ABILITY_100BASE_TX)

extern "C" {

yt_rc yt8531c_eee_get(yt8531c_dev *dev, yt8531c_eee *eee)
{
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev) || eee == nullptr)
		return YT8531C_ERR_ARG;

	yt8531c::detail::mem_zero(eee, sizeof(*eee));

	rc = yt8531c::detail::mmd_read(dev, YT8531C_MMD3, 0x0014u,
				       &eee->capabilities);
	if (rc != YT8531C_OK)
		return rc;

	rc = yt8531c::detail::mmd_read(dev, YT8531C_MMD7, 0x003cu,
				       &eee->local_ability);
	if (rc != YT8531C_OK)
		return rc;

	rc = yt8531c::detail::mmd_read(dev, YT8531C_MMD7, 0x003du,
				       &eee->lp_ability);
	if (rc != YT8531C_OK)
		return rc;

	rc = yt8531c::detail::mmd_read(dev, YT8531C_MMD3, 0x0001u,
				       &eee->pcs_status);
	if (rc != YT8531C_OK)
		return rc;

	/* Reading the wake error counter clears it. */
	return yt8531c::detail::mmd_read(dev, YT8531C_MMD3, 0x0016u,
					 &eee->wake_errors);
}

yt_rc yt8531c_eee_set(yt8531c_dev *dev, yt_u16 ability_mask)
{
	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;
	if ((ability_mask & static_cast<yt_u16>(~YT8531C_EEE_ABILITY_MASK)) != 0u)
		return YT8531C_ERR_ARG;

	return yt8531c::detail::mmd_write(
		dev, YT8531C_MMD7, 0x003cu,
		static_cast<yt_u16>(ability_mask & YT8531C_EEE_ABILITY_MASK));
}

} /* extern "C" */
