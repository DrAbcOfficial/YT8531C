/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only */
/*
 * YT8531C core: interrupt mask and read-clear event status.
 */

#include "yt8531c/yt8531c.h"

#include "yt8531c_internal.hpp"

extern "C" {

yt_rc yt8531c_irq_set_mask(yt8531c_dev *dev, yt_u16 mask)
{
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;
	if ((mask & static_cast<yt_u16>(~YT8531C_IRQ_MASK_ALL)) != 0u)
		return YT8531C_ERR_ARG;

	rc = yt8531c::detail::mii_write(dev, YT8531C_REG_INT_MASK, mask);
	if (rc == YT8531C_OK)
		dev->int_mask = mask;

	return rc;
}

yt_rc yt8531c_irq_get_mask(yt8531c_dev *dev, yt_u16 *mask)
{
	if (mask == nullptr)
		return YT8531C_ERR_ARG;

	return yt8531c::detail::mii_read(dev, YT8531C_REG_INT_MASK, mask);
}

yt_rc yt8531c_irq_read(yt8531c_dev *dev, yt_u16 *events)
{
	if (events == nullptr || !yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	/* The status register is read-clear, so one read acknowledges all. */
	return yt8531c::detail::mii_read(dev, YT8531C_REG_INT_STATUS, events);
}

yt_rc yt8531c_irq_clear(yt8531c_dev *dev)
{
	yt_u16 scratch = 0u;
	yt_rc rc;

	if (!yt8531c::detail::is_ready(dev))
		return YT8531C_ERR_ARG;

	rc = yt8531c::detail::mii_read(dev, YT8531C_REG_INT_STATUS, &scratch);
	if (rc != YT8531C_OK)
		return rc;

	/* A second read makes sure a source that re-asserted is also cleared. */
	return yt8531c::detail::mii_read(dev, YT8531C_REG_INT_STATUS, &scratch);
}

yt_rc yt8531c_irq_default_mask(yt_u16 *mask)
{
	if (mask == nullptr)
		return YT8531C_ERR_ARG;

	*mask = YT8531C_IRQ_MASK_DEFAULT;
	return YT8531C_OK;
}

} /* extern "C" */
