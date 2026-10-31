/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The machine's identity from SMBIOS (SMBIOS.cpp).
 */

#ifndef _MINKAR_SMBIOS_H_
#define _MINKAR_SMBIOS_H_

#include <IOKit/IOService.h>

/* Publishes the platform UUID, serial number and model on the platform. */
void MinkarSMBIOSPublish(IOService *platformDevice);

/* The product name SMBIOS gave, or NULL. */
const char *MinkarSMBIOSModel(void);

#endif /* _MINKAR_SMBIOS_H_ */
