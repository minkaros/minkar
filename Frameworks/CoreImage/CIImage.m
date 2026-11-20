#import <CoreImage/CIImage.h>
#import <CoreImage/CIFilter.h>
#import "CIImagePrivate.h"
#import <Foundation/NSRaise.h>

CIImage *CIImageWithFilter(CIFilter *filter) {
   CIImage *image=[[[CIImage alloc] initWithCGImage:NULL] autorelease];

   [image setFilter:filter];
   return image;
}

@implementation CIImage

+(CIImage *)emptyImage {
   return [[[self alloc] initWithCGImage:NULL] autorelease];
}

+(CIImage *)imageWithCGImage:(CGImageRef)cgImage {
   return [[[self alloc] initWithCGImage:cgImage] autorelease];
}

-initWithCGImage:(CGImageRef)cgImage {
   _cgImage=CGImageRetain(cgImage);
   _filter=nil;
   return self;
}

-(void)dealloc {
   CGImageRelease(_cgImage);
   [_filter release];
   [super dealloc];
}

/* Minkar: a filter's output covers what it draws: a gradient the whole
   plane, a blur its input and as far again as its radius. */
-(CGRect)extent {
   CGRect result;

   if(_filter!=nil){
    NSString *name=[_filter name];

    if([name isEqualToString:@"CILinearGradient"])
     return CGRectInfinite;
    if([name isEqualToString:@"CIBoxBlur"]){
     CIImage *input=[_filter valueForKey:@"inputImage"];
     CGFloat  radius=CIBoxBlurHalfWidth([[_filter valueForKey:@"inputRadius"] doubleValue]);

     return CGRectInset([input extent],-radius,-radius);
    }
   }
   if(_cgImage==NULL)
    return CGRectZero;
   
   result.origin.x=0;
   result.origin.y=0;
   result.size.width=CGImageGetWidth(_cgImage);
   result.size.height=CGImageGetHeight(_cgImage);

   return result;
}

-(CGImageRef)CGImage {
   return _cgImage;
}

-(CIFilter *)filter {
   return _filter;
}

-(void)setFilter:(CIFilter *)filter {
   filter=[filter retain];
   [_filter release];
   _filter=filter;
}

@end
