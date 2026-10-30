/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
// The CTF tools key DenseMap and DenseSet by C strings and mean their
// contents (Apple's own llvm-ADT copy says so): the same here, on top of
// LLVM's DenseMapInfo, which compares pointers.
#ifndef HOST_DENSEMAPINFO_CSTR_H
#define HOST_DENSEMAPINFO_CSTR_H
#include <llvm/ADT/DenseMapInfo.h>
#include <cstring>
#include <functional>
#include <string_view>

namespace llvm {
template <> struct DenseMapInfo<const char *> {
  static inline const char *getEmptyKey() {
    return reinterpret_cast<const char *>(-1);
  }
  static inline const char *getTombstoneKey() {
    return reinterpret_cast<const char *>(-2);
  }
  static unsigned getHashValue(const char *s) {
    return (unsigned)std::hash<std::string_view>()(std::string_view(s));
  }
  static bool isEqual(const char *a, const char *b) {
    if (a == b)
      return true;
    if (a == getEmptyKey() || b == getEmptyKey() ||
        a == getTombstoneKey() || b == getTombstoneKey())
      return false;
    return std::strcmp(a, b) == 0;
  }
};
}
#endif
