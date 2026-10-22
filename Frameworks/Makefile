# ------------------------------------------------------------------------
#  RAVYNOS BUILD SYSTEM - Frameworks
# ------------------------------------------------------------------------

SUBDIR = CoreFoundation
#SUBDIR += CFNetwork
SUBDIR += Foundation
SUBDIR += CoreServices
SUBDIR += Onyx2D
SUBDIR += CoreGraphics
SUBDIR += OpenGL
SUBDIR += CoreText
SUBDIR += CoreData
SUBDIR += QuartzCore
SUBDIR += AppKit

all: ${SUBDIR}

Foundation: CoreFoundation
CoreData: Foundation
QuartzCore: OpenGL
AppKit: CoreData QuartzCore

.include <bsd.subdir.mk>
