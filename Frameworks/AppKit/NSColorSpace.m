/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSColorSpace.h>
#import <AppKit/NSGraphics.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSKeyedArchiver.h>
#include <string.h>

@implementation NSColorSpace

+(BOOL)supportsSecureCoding {
   return YES;
}

static NSColorSpace *namedSpace(NSColorSpace **cache,CFStringRef name,NSString *localizedName){
   @synchronized([NSColorSpace class]){
    if(*cache==nil){
     CGColorSpaceRef cg=(name!=NULL)?CGColorSpaceCreateWithName(name):NULL;

     *cache=[[NSColorSpace alloc] initWithCGColorSpace:cg];
     (*cache)->_localizedName=[localizedName copy];
     CGColorSpaceRelease(cg);
    }
   }
   return *cache;
}

static NSColorSpace *deviceSpace(NSColorSpace **cache,CGColorSpaceRef (*create)(void),NSString *localizedName){
   @synchronized([NSColorSpace class]){
    if(*cache==nil){
     CGColorSpaceRef cg=create();

     *cache=[[NSColorSpace alloc] initWithCGColorSpace:cg];
     (*cache)->_localizedName=[localizedName copy];
     CGColorSpaceRelease(cg);
    }
   }
   return *cache;
}

+(NSColorSpace *)sRGBColorSpace {
   static NSColorSpace *space; return namedSpace(&space,kCGColorSpaceSRGB,@"sRGB IEC61966-2.1");
}
+(NSColorSpace *)extendedSRGBColorSpace {
   static NSColorSpace *space; return namedSpace(&space,kCGColorSpaceExtendedSRGB,@"Extended sRGB");
}
+(NSColorSpace *)displayP3ColorSpace {
   static NSColorSpace *space; return namedSpace(&space,kCGColorSpaceDisplayP3,@"Display P3");
}
+(NSColorSpace *)adobeRGB1998ColorSpace {
   static NSColorSpace *space; return namedSpace(&space,kCGColorSpaceAdobeRGB1998,@"Adobe RGB (1998)");
}
+(NSColorSpace *)genericRGBColorSpace {
   static NSColorSpace *space; return namedSpace(&space,kCGColorSpaceGenericRGB,@"Generic RGB");
}
+(NSColorSpace *)deviceRGBColorSpace {
   static NSColorSpace *space; return deviceSpace(&space,CGColorSpaceCreateDeviceRGB,@"Device RGB");
}
+(NSColorSpace *)genericGrayColorSpace {
   static NSColorSpace *space; return namedSpace(&space,kCGColorSpaceGenericGray,@"Generic Gray");
}
+(NSColorSpace *)genericGamma22GrayColorSpace {
   static NSColorSpace *space; return namedSpace(&space,kCGColorSpaceGenericGrayGamma2_2,@"Generic Gray Gamma 2.2");
}
+(NSColorSpace *)extendedGenericGamma22GrayColorSpace {
   static NSColorSpace *space; return namedSpace(&space,kCGColorSpaceExtendedGray,@"Extended Gray");
}
+(NSColorSpace *)deviceGrayColorSpace {
   static NSColorSpace *space; return deviceSpace(&space,CGColorSpaceCreateDeviceGray,@"Device Gray");
}
+(NSColorSpace *)genericCMYKColorSpace {
   static NSColorSpace *space; return namedSpace(&space,kCGColorSpaceGenericCMYK,@"Generic CMYK");
}
+(NSColorSpace *)deviceCMYKColorSpace {
   static NSColorSpace *space; return deviceSpace(&space,CGColorSpaceCreateDeviceCMYK,@"Device CMYK");
}

+(NSArray *)availableColorSpacesWithModel:(NSColorSpaceModel)model {
   NSArray *all=[NSArray arrayWithObjects:[self sRGBColorSpace],[self extendedSRGBColorSpace],[self displayP3ColorSpace],
      [self adobeRGB1998ColorSpace],[self genericRGBColorSpace],[self genericGrayColorSpace],[self genericGamma22GrayColorSpace],
      [self extendedGenericGamma22GrayColorSpace],[self genericCMYKColorSpace],nil];
   NSMutableArray *result=[NSMutableArray array];

   for(NSColorSpace *space in all)
    if(model==NSColorSpaceModelUnknown || [space colorSpaceModel]==model)
     [result addObject:space];
   return result;
}

-initWithCGColorSpace:(CGColorSpaceRef)cgColorSpace {
   if(cgColorSpace==NULL){
    [self release];
    return nil;
   }
   _cgColorSpace=CGColorSpaceRetain(cgColorSpace);
   return self;
}

/* Without color management a profile's model is all that can be kept;
   nil for data that is not an ICC profile. */
-initWithICCProfileData:(NSData *)iccData {
   const unsigned char *bytes=[iccData bytes];
   CGColorSpaceRef      cg=NULL;

   if([iccData length]<40 || memcmp(bytes+36,"acsp",4)!=0){
    [self release];
    return nil;
   }
   if(memcmp(bytes+16,"RGB ",4)==0)
    cg=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
   else if(memcmp(bytes+16,"GRAY",4)==0)
    cg=CGColorSpaceCreateWithName(kCGColorSpaceGenericGray);
   else if(memcmp(bytes+16,"CMYK",4)==0)
    cg=CGColorSpaceCreateWithName(kCGColorSpaceGenericCMYK);
   self=[self initWithCGColorSpace:cg];
   CGColorSpaceRelease(cg);
   return self;
}

-(void)dealloc {
   CGColorSpaceRelease(_cgColorSpace);
   [_localizedName release];
   [super dealloc];
}

-(CGColorSpaceRef)CGColorSpace {
   return _cgColorSpace;
}

-(NSColorSpaceModel)colorSpaceModel {
   switch(CGColorSpaceGetModel(_cgColorSpace)){
    case kCGColorSpaceModelMonochrome: return NSColorSpaceModelGray;
    case kCGColorSpaceModelRGB:        return NSColorSpaceModelRGB;
    case kCGColorSpaceModelCMYK:       return NSColorSpaceModelCMYK;
    case kCGColorSpaceModelLab:        return NSColorSpaceModelLAB;
    case kCGColorSpaceModelDeviceN:    return NSColorSpaceModelDeviceN;
    case kCGColorSpaceModelIndexed:    return NSColorSpaceModelIndexed;
    case kCGColorSpaceModelPattern:    return NSColorSpaceModelPatterned;
    default:                           return NSColorSpaceModelUnknown;
   }
}

-(NSInteger)numberOfColorComponents {
   return CGColorSpaceGetNumberOfComponents(_cgColorSpace);
}

-(NSString *)localizedName {
   static const struct { const char *name,*localized; } names[]={
    {"kCGColorSpaceSRGB","sRGB IEC61966-2.1"},{"kCGColorSpaceExtendedSRGB","Extended sRGB"},
    {"kCGColorSpaceDisplayP3","Display P3"},{"kCGColorSpaceAdobeRGB1998","Adobe RGB (1998)"},
    {"kCGColorSpaceGenericRGB","Generic RGB"},{"kCGColorSpaceDeviceRGB","Device RGB"},
    {"kCGColorSpaceGenericGray","Generic Gray"},{"kCGColorSpaceGenericGrayGamma2_2","Generic Gray Gamma 2.2"},
    {"kCGColorSpaceExtendedGray","Extended Gray"},{"kCGColorSpaceDeviceGray","Device Gray"},
    {"kCGColorSpaceGenericCMYK","Generic CMYK"},{"kCGColorSpaceDeviceCMYK","Device CMYK"},
   };
   NSString *name=(NSString *)CGColorSpaceGetName(_cgColorSpace);
   size_t    i;

   if(_localizedName!=nil)
    return _localizedName;
   for(i=0;i<sizeof(names)/sizeof(names[0]);i++)
    if([name isEqualToString:[NSString stringWithUTF8String:names[i].name]])
     return [NSString stringWithUTF8String:names[i].localized];
   return name;
}

-(NSData *)ICCProfileData {
   return nil;
}

-(NSString *)_colorSpaceName {
   CFStringRef name=CGColorSpaceGetName(_cgColorSpace);
   BOOL        device=(name==NULL || CFStringHasPrefix(name,CFSTR("kCGColorSpaceDevice")));

   switch([self colorSpaceModel]){
    case NSColorSpaceModelGray: return device?NSDeviceWhiteColorSpace:NSCalibratedWhiteColorSpace;
    case NSColorSpaceModelCMYK: return NSDeviceCMYKColorSpace;
    default:                    return device?NSDeviceRGBColorSpace:NSCalibratedRGBColorSpace;
   }
}

-(BOOL)isEqual:(id)other {
   if(other==self)
    return YES;
   if(![other isKindOfClass:[NSColorSpace class]])
    return NO;
   CFStringRef a=CGColorSpaceGetName(_cgColorSpace),b=CGColorSpaceGetName([other CGColorSpace]);
   return a!=NULL && b!=NULL && CFEqual(a,b);
}

-(NSUInteger)hash {
   CFStringRef name=CGColorSpaceGetName(_cgColorSpace);

   return (name!=NULL)?CFHash(name):(NSUInteger)_cgColorSpace;
}

/* Kept by the name of the CG space. */
-(void)encodeWithCoder:(NSCoder *)coder {
   [coder encodeObject:(NSString *)CGColorSpaceGetName(_cgColorSpace) forKey:@"NSColorSpaceName"];
}

-initWithCoder:(NSCoder *)coder {
   NSString       *name=[coder decodeObjectOfClass:[NSString class] forKey:@"NSColorSpaceName"];
   CGColorSpaceRef cg=NULL;

   if([name isEqualToString:@"kCGColorSpaceDeviceRGB"])
    cg=CGColorSpaceCreateDeviceRGB();
   else if([name isEqualToString:@"kCGColorSpaceDeviceGray"])
    cg=CGColorSpaceCreateDeviceGray();
   else if([name isEqualToString:@"kCGColorSpaceDeviceCMYK"])
    cg=CGColorSpaceCreateDeviceCMYK();
   else if(name!=nil)
    cg=CGColorSpaceCreateWithName((CFStringRef)name);
   self=[self initWithCGColorSpace:cg];
   CGColorSpaceRelease(cg);
   return self;
}

-(NSString *)description {
   return [NSString stringWithFormat:@"%@ colorspace",[self localizedName]];
}

@end
