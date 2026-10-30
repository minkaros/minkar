// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// What the CTF tools' DenseMap takes from LLVMSupport, for their Linux build.
#include <cstddef>
#include <new>

namespace llvm {
int DisableABIBreakingChecks = 1;

void *
allocate_buffer(size_t size, size_t alignment)
{
	return ::operator new(size, std::align_val_t(alignment));
}

void
deallocate_buffer(void *ptr, size_t size, size_t alignment)
{
	(void)size;
	::operator delete(ptr, std::align_val_t(alignment));
}
}
