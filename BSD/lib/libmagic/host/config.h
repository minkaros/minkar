/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Minkar: the configuration for mkmagic, the magic compiler built for
 * the build host (Linux or macOS) to make magic.mgc: Minkar's own
 * config.h, less what the host's libc may lack.
 */
#include "../config.h"

#undef HAVE_XLOCALE_H
#undef HAVE_FMTCHECK
#undef HAVE_STRLCPY
#undef HAVE_STRLCAT
#undef HAVE_USELOCALE
#undef HAVE_NEWLOCALE
#undef HAVE_FREELOCALE
