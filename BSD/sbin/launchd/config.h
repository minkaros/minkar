#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <TargetConditionals.h>

#if __has_include(<quarantine.h>)
#define HAVE_QUARANTINE 1
#else
#define HAVE_QUARANTINE 0
#endif

#if __has_include(<responsibility.h>)
#define HAVE_RESPONSIBILITY 1
#else
#define HAVE_RESPONSIBILITY 0
#endif

#if __has_include(<sandbox.h>)
#define HAVE_SANDBOX 1
#else
#define HAVE_SANDBOX 0
#endif

/* libbsm and libauditd (OpenBSM, Libraries/openbsm). Without them launchd
 * uses only the audit system calls, declared in <bsm/audit.h>. */
#if __has_include(<bsm/libbsm.h>)
#define HAVE_LIBBSM 1
#else
#define HAVE_LIBBSM 0
#endif

#if !TARGET_OS_EMBEDDED && __has_include(<bsm/auditd_lib.h>)
#define HAVE_LIBAUDITD 1
#else
#define HAVE_LIBAUDITD 0
#endif

#if !TARGET_OS_EMBEDDED && __has_include(<systemstats/systemstats.h>)
#define HAVE_SYSTEMSTATS 1
#else
#define HAVE_SYSTEMSTATS 0
#endif

/* The xpc_domain MIG subsystem (Apple's libxpc talks to launchd through it);
 * Minkar's libxpc does not use it and domain.defs is not available. */
#define HAVE_XPC_DOMAIN 0

/* XPC pipes: the XPC side of launchd's receive loop and the XPC event and
 * process routines. Minkar's libxpc does not implement them yet. */
#define HAVE_XPC_PIPE 0

#endif /* __CONFIG_H__ */
