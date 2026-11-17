/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// Original - Christopher Lloyd <cjwl@objc.net>
#import "NSCustomObject.h"
#import <Foundation/NSString.h>
#import <Foundation/NSException.h>
#import <Foundation/NSKeyedArchiver.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSFontManager.h>

/* A class named in a nib: its name, and the module it is in (a Swift
   module) when it has one. */
@implementation IBClassReference

-initWithCoder:(NSCoder *)coder {
   if([coder allowsKeyedCoding]){
    _className=[[coder decodeObjectForKey:@"IBClassName"] copy];
    _moduleName=[[coder decodeObjectForKey:@"IBModuleName"] copy];
   }
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   if([coder allowsKeyedCoding]){
    [coder encodeObject:_className forKey:@"IBClassName"];
    [coder encodeObject:_moduleName forKey:@"IBModuleName"];
   }
}

-(void)dealloc {
   [_className release];
   [_moduleName release];
   [super dealloc];
}

-(NSString *)className {
   return _className;
}

-(NSString *)moduleName {
   return _moduleName;
}

/* The name NSClassFromString finds: the class alone, or for a module
   "Module.Class", or the Objective-C name Swift gives such a class. */
-(NSString *)_runtimeClassName {
   if([_moduleName length]==0)
    return _className;
   NSString *dotted=[NSString stringWithFormat:@"%@.%@",_moduleName,_className];
   if(NSClassFromString(dotted)!=Nil)
    return dotted;
   NSString *mangled=[NSString stringWithFormat:@"_TtC%lu%@%lu%@",(unsigned long)[_moduleName length],_moduleName,
    (unsigned long)[_className length],_className];
   if(NSClassFromString(mangled)!=Nil)
    return mangled;
   return _className;
}

@end

@implementation NSCustomObject

-initWithCoder:(NSCoder *)coder {
   if([coder allowsKeyedCoding]){
    NSKeyedUnarchiver *keyed=(NSKeyedUnarchiver *)coder;

    _className=[[keyed decodeObjectForKey:@"NSClassName"] retain];
    /* Newer nibs name the class by an IBClassReference (class and
       module) instead. */
    if(_className==nil){
     IBClassReference *ref=[keyed decodeObjectForKey:@"IBClassReference"];

     if([ref isKindOfClass:[IBClassReference class]])
      _className=[[ref _runtimeClassName] retain];
    }
   }
   else
    [NSException raise:NSInvalidArgumentException format:@"-[%@ %s] does not handle %@",[self class],sel_getName(_cmd),[coder class]];

   return self;
}

-(void)dealloc {
   [_className release];
   [super dealloc];
}

-(id)createCustomInstance {
   Class class=NSClassFromString(_className);
   id ret=nil;

   // Minkar: an object of a class the program does not have is an NSObject,
   // as on the reference platform (it was nil, which nibs cannot hold).
   if(class==Nil){
    NSLog(@"Unknown class %@ in Interface Builder file.",_className);
    class=[NSObject class];
   }

   if([_className isEqualToString:@"NSApplication"]) {
      ret=[[NSApplication sharedApplication] retain];

   } else if ([_className isEqualToString: @"NSFontManager"]) {
       ret = [[NSFontManager sharedFontManager] retain];

   // Make sure we don't create some other object that should be shared already (for
   // example the NSColorPanel).
   // This is a bit fragile because if it's not created yet then this will still create
   // a duplicate. The only alternative is to specifically handle the shared objects here
   // like NSApplication and NSFontManager above... Perhaps that's more explicit and it
   // prevents the race condition - but seems quite high maintenance
   } else if (NSThreadSharedInstanceDoNotCreate(_className) != nil) {
       ret = [NSThreadSharedInstanceDoNotCreate(_className) retain];
   }
   else {
      ret=[[class alloc] init];
   }

   return ret;
}

#if 0
-awakeAfterUsingCoder:(NSCoder *)coder {
   Class class=NSClassFromString(_className);
   id ret=nil;

   if(class==Nil)
    NSLog(@"NSCustomObject unknown class %@",_className);

   if([_className isEqualToString:@"NSApplication"]) {
      ret=[[NSApplication sharedApplication] retain];
   }
   else {
      ret=[[class alloc] init];
   }
   [self release];

   return ret;
}
#endif

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@:%p:class name=%@>",[self class],self,_className];
}

@end
