# ------------------------------------------------------------------------
#  MINKAR BUILD SYSTEM - Frameworks
# ------------------------------------------------------------------------

SUBDIR = CoreFoundation
# IOKit.framework: it links CoreFoundation, and SystemConfiguration links it.
SUBDIR += ../Kernel/IOKitUser
#SUBDIR += CFNetwork
SUBDIR += SystemConfiguration
SUBDIR += Foundation
SUBDIR += CoreServices
SUBDIR += Onyx2D
# CoreGraphics links OpenGL.
SUBDIR += OpenGL
SUBDIR += CoreGraphics
SUBDIR += CoreText
SUBDIR += CoreData
SUBDIR += QuartzCore
SUBDIR += AppKit

all: ${SUBDIR}

Foundation: CoreFoundation
SystemConfiguration: CoreFoundation
CoreData: Foundation
QuartzCore: OpenGL
AppKit: CoreData QuartzCore

.include <bsd.subdir.mk>
