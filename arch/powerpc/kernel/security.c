// SPDX-License-Identifier: GPL-2.0+
//
// Security related flags and so on.
//
// Copyright 2018, Michael Ellerman, IBM Corporation.

#include <linux/kernel.h>
#include <linux/device.h>

#include <asm/security_features.h>


unsigned long powerpc_security_features __read_mostly = SEC_FTR_DEFAULT;

ssize_t cpu_show_meltdown(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (rfi_flush)
		return sprintf(buf, "Mitigation: RFI Flush\n");

	return sprintf(buf, "Vulnerable\n");
}

ssize_t cpu_show_spectre_v1(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!security_ftr_enabled(SEC_FTR_BNDS_CHK_SPEC_BAR))
		return sprintf(buf, "Not affected\n");

	return sprintf(buf, "Vulnerable\n");
}
