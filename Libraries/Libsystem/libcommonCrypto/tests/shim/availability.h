/* Host test shim: the SDK's availability macros, as no-ops. */
#ifndef API_AVAILABLE
#define API_AVAILABLE(...)
#define API_DEPRECATED(...)
#define API_DEPRECATED_WITH_REPLACEMENT(...)
#define API_UNAVAILABLE(...)
#define __OSX_AVAILABLE_STARTING(...)
#define __OSX_AVAILABLE_BUT_DEPRECATED(...)
#define __IPHONE_NA
#endif
