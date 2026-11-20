#import <AppKit/NSImageRep.h>
#import <CoreImage/CIImage.h>

@class NSBitmapImageRep;

@interface CIImage (CIImageRepAdditions)
- initWithBitmapImageRep:(NSBitmapImageRep *)bitmapImageRep;
@end
