#import <CoreGraphics/CGImageSource.h>
#import <Onyx2D/O2ImageSource.h>
#import <CoreGraphics/CGImageProperties.h>
#import <CoreGraphics/CGImage.h>
#import <CoreGraphics/CGBitmapContext.h>
#import <CoreGraphics/CGContext.h>
#import <Foundation/Foundation.h>

@interface _O2ImageSource : O2ImageSource
@end

/* Data no decoder reads: a source of no images, whose status says so. */
@interface _CGUnknownImageSource : O2ImageSource
@end

@implementation _CGUnknownImageSource
-(CFStringRef)type { return NULL; }
-(unsigned)count { return 0; }
-(O2Image *)createImageAtIndex:(unsigned)index options:(CFDictionaryRef)options { return nil; }
@end

@implementation O2ImageSource (CGImageSource)
-(CFTypeID)_cfTypeID { return CGImageSourceGetTypeID(); }
-(size_t)_dataLength { return [_provider length]; }
@end

CGImageSourceRef CGImageSourceCreateWithData(CFDataRef data,CFDictionaryRef options) {
   O2ImageSource *source=[O2ImageSource newImageSourceWithData:data options:options];

   if(source==nil && data!=NULL){
    O2DataProviderRef provider=O2DataProviderCreateWithCFData(data);

    source=[[_CGUnknownImageSource alloc] initWithDataProvider:provider options:(NSDictionary *)options];
    O2DataProviderRelease(provider);
   }
   return (CGImageSourceRef)source;
}

CGImageSourceRef CGImageSourceCreateWithURL(CFURLRef url,CFDictionaryRef options) {
	return (CGImageSourceRef)[O2ImageSource newImageSourceWithURL:(NSURL *)url options:options];
}

size_t CGImageSourceGetCount(CGImageSourceRef self) {
   return [self count];
}

CGImageRef CGImageSourceCreateImageAtIndex(CGImageSourceRef self,size_t index,CFDictionaryRef options) {
   return [self createImageAtIndex:index options:options];
}

static NSDictionary *formatDictionaryKeys(void) {
   static NSDictionary *keys;

   if(keys==nil)
    keys=[[NSDictionary alloc] initWithObjectsAndKeys:
       (NSString *)kCGImagePropertyPNGDictionary,@"public.png",
       (NSString *)kCGImagePropertyJFIFDictionary,@"public.jpeg",
       (NSString *)kCGImagePropertyTIFFDictionary,@"public.tiff",
       (NSString *)kCGImagePropertyGIFDictionary,@"com.compuserve.gif",
       nil];
   return keys;
}

/* The decoder's properties, and the image's size, depth, alpha and color
   model where it gives none (PNG and BMP give none). */
CFDictionaryRef CGImageSourceCopyPropertiesAtIndex(CGImageSourceRef self, size_t index,CFDictionaryRef options) {
   NSDictionary        *decoded=[(NSDictionary *)[self copyPropertiesAtIndex:index options:options] autorelease];
   NSMutableDictionary *result=(decoded!=nil)?[[decoded mutableCopy] autorelease]:[NSMutableDictionary dictionary];
   CGImageRef           image;

   if(index>=CGImageSourceGetCount(self))
    return NULL;
   if([result objectForKey:(NSString *)kCGImagePropertyPixelWidth]==nil || [result objectForKey:(NSString *)kCGImagePropertyDepth]==nil ||
      [result objectForKey:(NSString *)kCGImagePropertyHasAlpha]==nil || [result objectForKey:(NSString *)kCGImagePropertyColorModel]==nil){
    image=[self createImageAtIndex:index options:options];
    if(image!=NULL){
     CGImageAlphaInfo alpha=CGImageGetAlphaInfo(image);
     CGColorSpaceRef  space=CGImageGetColorSpace(image);
     NSString        *model=(NSString *)kCGImagePropertyColorModelRGB;

     if(space!=NULL && CGColorSpaceGetModel(space)==kCGColorSpaceModelMonochrome)
      model=(NSString *)kCGImagePropertyColorModelGray;
     else if(space!=NULL && CGColorSpaceGetModel(space)==kCGColorSpaceModelCMYK)
      model=(NSString *)kCGImagePropertyColorModelCMYK;
     if([result objectForKey:(NSString *)kCGImagePropertyPixelWidth]==nil){
      [result setObject:[NSNumber numberWithUnsignedLong:CGImageGetWidth(image)] forKey:(NSString *)kCGImagePropertyPixelWidth];
      [result setObject:[NSNumber numberWithUnsignedLong:CGImageGetHeight(image)] forKey:(NSString *)kCGImagePropertyPixelHeight];
     }
     if([result objectForKey:(NSString *)kCGImagePropertyDepth]==nil)
      [result setObject:[NSNumber numberWithUnsignedLong:CGImageGetBitsPerComponent(image)] forKey:(NSString *)kCGImagePropertyDepth];
     if([result objectForKey:(NSString *)kCGImagePropertyHasAlpha]==nil)
      [result setObject:[NSNumber numberWithBool:alpha!=kCGImageAlphaNone && alpha!=kCGImageAlphaNoneSkipFirst && alpha!=kCGImageAlphaNoneSkipLast]
         forKey:(NSString *)kCGImagePropertyHasAlpha];
     if([result objectForKey:(NSString *)kCGImagePropertyColorModel]==nil)
      [result setObject:model forKey:(NSString *)kCGImagePropertyColorModel];
     CGImageRelease(image);
    }
   }
   /* The format's own dictionary, empty where the decoder tells nothing. */
   NSString *formatKey=[formatDictionaryKeys() objectForKey:(NSString *)CGImageSourceGetType(self)];

   if(formatKey!=nil && [result objectForKey:formatKey]==nil)
    [result setObject:[NSDictionary dictionary] forKey:formatKey];
   return (CFDictionaryRef)[result retain];
}

CFStringRef CGImageSourceGetType(CGImageSourceRef self)
{
    return [self type];
}

const CFStringRef kCGImageSourceTypeIdentifierHint=(CFStringRef)@"kCGImageSourceTypeIdentifierHint";
const CFStringRef kCGImageSourceShouldCache=(CFStringRef)@"kCGImageSourceShouldCache";
const CFStringRef kCGImageSourceShouldCacheImmediately=(CFStringRef)@"kCGImageSourceShouldCacheImmediately";
const CFStringRef kCGImageSourceShouldAllowFloat=(CFStringRef)@"kCGImageSourceShouldAllowFloat";
const CFStringRef kCGImageSourceCreateThumbnailFromImageIfAbsent=(CFStringRef)@"kCGImageSourceCreateThumbnailFromImageIfAbsent";
const CFStringRef kCGImageSourceCreateThumbnailFromImageAlways=(CFStringRef)@"kCGImageSourceCreateThumbnailFromImageAlways";
const CFStringRef kCGImageSourceThumbnailMaxPixelSize=(CFStringRef)@"kCGImageSourceThumbnailMaxPixelSize";
const CFStringRef kCGImageSourceCreateThumbnailWithTransform=(CFStringRef)@"kCGImageSourceCreateThumbnailWithTransform";
const CFStringRef kCGImageSourceSubsampleFactor=(CFStringRef)@"kCGImageSourceSubsampleFactor";

/* A CF type registered for image sources (they are Objective-C objects). */
extern CFTypeID _CFRuntimeRegisterClass(const void *runtimeClass);

static const struct { CFIndex version; const char *className; void *functions[16]; } imageSourceClass={ 0, "CGImageSource", { NULL } };

CFTypeID CGImageSourceGetTypeID(void) {
   static CFTypeID typeID;

   if(typeID==0)
    typeID=_CFRuntimeRegisterClass(&imageSourceClass);
   return typeID;
}

CFArrayRef CGImageSourceCopyTypeIdentifiers(void) {
   return (CFArrayRef)[[NSArray alloc] initWithObjects:@"public.png",@"public.jpeg",@"com.compuserve.gif",@"public.tiff",
      @"com.microsoft.bmp",@"com.apple.icns",nil];
}

CFDictionaryRef CGImageSourceCopyProperties(CGImageSourceRef self, CFDictionaryRef options) {
   NSMutableDictionary *result=[[NSMutableDictionary alloc] init];
   NSDictionary        *first=[(NSDictionary *)CGImageSourceCopyPropertiesAtIndex(self,0,options) autorelease];

   NSDictionary        *gif=[first objectForKey:(NSString *)kCGImagePropertyGIFDictionary];

   if(gif!=nil)
    [result setObject:gif forKey:(NSString *)kCGImagePropertyGIFDictionary];
   [result setObject:[NSNumber numberWithUnsignedLong:[(O2ImageSource *)self _dataLength]] forKey:(NSString *)kCGImagePropertyFileSize];
   return (CFDictionaryRef)result;
}

CGImageRef CGImageSourceCreateThumbnailAtIndex(CGImageSourceRef self, size_t index, CFDictionaryRef options) {
   CGImageRef image=CGImageSourceCreateImageAtIndex(self,index,options);
   NSNumber  *maximum=[(NSDictionary *)options objectForKey:(NSString *)kCGImageSourceThumbnailMaxPixelSize];
   size_t     width,height,longest;
   CGFloat    scale;

   if(image==NULL || maximum==nil)
    return image;
   width=CGImageGetWidth(image);
   height=CGImageGetHeight(image);
   longest=MAX(width,height);
   if(longest<=(size_t)[maximum unsignedLongValue] || longest==0)
    return image;
   scale=[maximum doubleValue]/longest;

   size_t          w=MAX(1,(size_t)(width*scale+0.5)),h=MAX(1,(size_t)(height*scale+0.5));
   CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
   CGContextRef    context=CGBitmapContextCreate(NULL,w,h,8,w*4,space,kCGImageAlphaPremultipliedLast);
   CGImageRef      thumbnail=NULL;

   if(context!=NULL){
    CGContextDrawImage(context,CGRectMake(0,0,w,h),image);
    thumbnail=CGBitmapContextCreateImage(context);
    CGContextRelease(context);
   }
   CGColorSpaceRelease(space);
   CGImageRelease(image);
   return thumbnail;
}

/* Sources are made from whole data: complete, or unknown when the data
   is no image. */
CGImageSourceStatus CGImageSourceGetStatus(CGImageSourceRef self) {
   return (self!=NULL && CGImageSourceGetCount(self)>0)?kCGImageStatusComplete:kCGImageStatusUnknownType;
}

CGImageSourceStatus CGImageSourceGetStatusAtIndex(CGImageSourceRef self, size_t index) {
   return (self!=NULL && index<CGImageSourceGetCount(self))?kCGImageStatusComplete:kCGImageStatusInvalidData;
}
