#if __has_include(<CFNetwork/CFNetwork.h>)
#import <CFNetwork/CFNetwork.h>
#endif
/* The sub-frameworks: from the SDK, or beside this header when it is
 * read from the source tree (-I.../Frameworks). */
#if __has_include(<FSEvents/FSEvents.h>)
#include <FSEvents/FSEvents.h>
#include <LaunchServices/LaunchServices.h>
#else
#include "FSEvents/FSEvents.h"
#include "LaunchServices/LaunchServices.h"
#endif
#include <CoreServices/Metadata.h>
