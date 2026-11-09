/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSAppearance.h>
#import <Foundation/Foundation.h>

NSAppearanceName const NSAppearanceNameAqua=@"NSAppearanceNameAqua";
NSAppearanceName const NSAppearanceNameDarkAqua=@"NSAppearanceNameDarkAqua";
NSAppearanceName const NSAppearanceNameLightContent=@"NSAppearanceNameLightContent";
NSAppearanceName const NSAppearanceNameVibrantDark=@"NSAppearanceNameVibrantDark";
NSAppearanceName const NSAppearanceNameVibrantLight=@"NSAppearanceNameVibrantLight";
NSAppearanceName const NSAppearanceNameAccessibilityHighContrastAqua=@"NSAppearanceNameAccessibilityAqua";
NSAppearanceName const NSAppearanceNameAccessibilityHighContrastDarkAqua=@"NSAppearanceNameAccessibilityDarkAqua";
NSAppearanceName const NSAppearanceNameAccessibilityHighContrastVibrantLight=@"NSAppearanceNameAccessibilityVibrantLight";
NSAppearanceName const NSAppearanceNameAccessibilityHighContrastVibrantDark=@"NSAppearanceNameAccessibilityVibrantDark";

static NSAppearance *currentAppearance;

@implementation NSAppearance

+(BOOL)supportsSecureCoding {
   return YES;
}

static NSArray *knownNames(void) {
   return [NSArray arrayWithObjects:NSAppearanceNameAqua,NSAppearanceNameDarkAqua,NSAppearanceNameLightContent,
      NSAppearanceNameVibrantDark,NSAppearanceNameVibrantLight,NSAppearanceNameAccessibilityHighContrastAqua,
      NSAppearanceNameAccessibilityHighContrastDarkAqua,NSAppearanceNameAccessibilityHighContrastVibrantLight,
      NSAppearanceNameAccessibilityHighContrastVibrantDark,nil];
}

/* One object per name. */
+(NSAppearance *)appearanceNamed:(NSAppearanceName)name {
   static NSMutableDictionary *appearances;
   NSAppearance *result;

   if(name==nil || ![knownNames() containsObject:name])
    return nil;
   @synchronized(self){
    if(appearances==nil)
     appearances=[[NSMutableDictionary alloc] init];
    result=[appearances objectForKey:name];
    if(result==nil){
     result=[[[NSAppearance alloc] init] autorelease];
     result->_name=[name copy];
     [appearances setObject:result forKey:name];
    }
   }
   return result;
}

/* Appearances from a bundle's asset catalogs are not read: the system's
   names only. */
-(instancetype)initWithAppearanceNamed:(NSAppearanceName)name bundle:(NSBundle *)bundle {
   NSAppearance *shared=[NSAppearance appearanceNamed:name];

   [self release];
   return [shared retain];
}

-(void)dealloc {
   [_name release];
   [super dealloc];
}

+(NSAppearance *)currentAppearance {
   return (currentAppearance!=nil)?currentAppearance:[self appearanceNamed:NSAppearanceNameAqua];
}

+(void)setCurrentAppearance:(NSAppearance *)appearance {
   appearance=[appearance retain];
   [currentAppearance release];
   currentAppearance=appearance;
}

-(NSAppearanceName)name {
   return _name;
}

-(BOOL)allowsVibrancy {
   return [_name rangeOfString:@"Vibrant"].location!=NSNotFound;
}

/* An exact match first, then the light or dark family it belongs to. */
-(NSAppearanceName)bestMatchFromAppearancesWithNames:(NSArray *)appearances {
   BOOL dark=[_name rangeOfString:@"Dark"].location!=NSNotFound;

   if([appearances containsObject:_name])
    return _name;
   for(NSAppearanceName name in appearances){
    BOOL other=[name rangeOfString:@"Dark"].location!=NSNotFound;

    if(other==dark && [knownNames() containsObject:name])
     return name;
   }
   return nil;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [coder encodeObject:_name forKey:@"NSAppearanceName"];
}

-initWithCoder:(NSCoder *)coder {
   NSString *name=[coder decodeObjectOfClass:[NSString class] forKey:@"NSAppearanceName"];

   [self release];
   return [[NSAppearance appearanceNamed:(name!=nil)?name:NSAppearanceNameAqua] retain];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<NSAppearance %@>",_name];
}

@end
