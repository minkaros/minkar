//===----------------------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The Minkar contributors
//
//===----------------------------------------------------------------------===//

// Minkar: libc++ is part of libSystem, which runs no static initializers;
// libSystem_initializer calls __libcxx_init to do their work.

extern "C" {
void __libcxx_ios_init(void) noexcept;
void __libcxx_init(void);
}

// libc++ is built with hidden visibility; libSystem calls this one.
__attribute__((visibility("default"))) void __libcxx_init(void) {
#if defined(__MINKAR__)
  __libcxx_ios_init();
#endif
}
