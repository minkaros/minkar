#if __has_include(<CFNetwork/CFNetwork.h>)
#import <CFNetwork/CFNetwork.h>
#endif
/* The sub-frameworks: from the SDK, or beside this header when it is
 * read from the source tree (-I.../Frameworks). */
#if __has_include(<FSEvents/FSEvents.h>)
#include <FSEvents/FSEvents.h>
#include <LaunchServices/LaunchServices.h>
#include <AE/AE.h>
#else
#include "FSEvents/FSEvents.h"
#include "LaunchServices/LaunchServices.h"
#include "AE/AE.h"
#endif
/* The classic managers still in use (CarbonCore). */
#include <CoreServices/MacErrors.h>
#include <CoreServices/MacMemory.h>
#include <CoreServices/Files.h>
#include <CoreServices/Resources.h>
#include <CoreServices/Gestalt.h>
#include <CoreServices/IconsCore.h>
#include <CoreServices/LSItemInfo.h>
#include <CoreServices/Metadata.h>
