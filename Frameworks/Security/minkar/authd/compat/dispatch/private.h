/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * libdispatch's private header: the public one, and the destructor that
 * hands a buffer back with vm_deallocate (libdispatch exports it).
 */

#ifndef MINKAR_AUTHD_DISPATCH_PRIVATE_H
#define MINKAR_AUTHD_DISPATCH_PRIVATE_H

#include <dispatch/dispatch.h>

#ifndef DISPATCH_DATA_DESTRUCTOR_VM_DEALLOCATE
extern const dispatch_block_t _dispatch_data_destructor_vm_deallocate;
#define DISPATCH_DATA_DESTRUCTOR_VM_DEALLOCATE (_dispatch_data_destructor_vm_deallocate)
#endif

#endif
