/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSNib.h>
#import "NSNibArchiveUnarchiver.h"
#import <Foundation/NSURL.h>
#import <AppKit/NSRaise.h>
#import <Foundation/NSKeyedArchiver.h>
#import <AppKit/NSNibLoading.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSTableCornerView.h>
#import "NSIBObjectData.h"
#import "NSNibHelpConnector.h"
#import "NSCustomObject.h"

NSString * const NSNibOwner=@"NSOwner";
NSString * const NSNibTopLevelObjects=@"NSNibTopLevelObjects";

@implementation NSNib

-initWithContentsOfFile:(NSString *)path {

    NIBDEBUG(@"initWithContentsOfFile: %@", path);
    
   NSString *keyedobjects=path;
   BOOL      isDirectory=NO;

	if([[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory] && isDirectory){
    keyedobjects=[[path stringByAppendingPathComponent:@"keyedobjects"] stringByAppendingPathExtension:@"nib"];
    // Minkar: a nib bundle of today's Interface Builder has its objects
    // as keyedobjects-<version>.nib (binary nibs) beside, or instead of,
    // keyedobjects.nib: the latest version's is taken.
    if(![[NSFileManager defaultManager] fileExistsAtPath:keyedobjects]){
     NSArray *names=[[[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:NULL] sortedArrayUsingSelector:@selector(compare:)];

     for(NSString *name in names)
      if([name hasPrefix:@"keyedobjects"] && [[name pathExtension] isEqualToString:@"nib"])
       keyedobjects=[path stringByAppendingPathComponent:name];
    }
   }
   
   if(!keyedobjects && !isDirectory)
      keyedobjects=path; // assume new-style compiled xib
   
   if((_data=[[NSData alloc] initWithContentsOfFile:keyedobjects])==nil){
    [self release];
    return nil;
   }

   _allObjects=[NSMutableArray new];
   
   return self;
}

-initWithContentsOfURL:(NSURL *)url {

    NIBDEBUG(@"initWithContentsOfURL: %@", url);

    if(![url isFileURL]){
    [self release];
    return nil;
   }
   
   return [self initWithContentsOfFile:[url path]];
}

-initWithNibNamed:(NSString *)name bundle:(NSBundle *)bundle {

    NIBDEBUG(@"initWithNibNamed: %@ bundle: %@", name, bundle);

    if(bundle==nil)
    bundle = [NSBundle mainBundle];
    
   NSString *path=[bundle pathForResource:name ofType:@"nib"];
   
   if(path==nil){
    NSLog(@"%s: unable to init nib with name '%@'", __PRETTY_FUNCTION__, name);
    [self release];
    return nil;
   }
   
   return [self initWithContentsOfFile:path];
}

-(instancetype)initWithNibData:(NSData *)nibData bundle:(NSBundle *)bundle {
   if(nibData==nil){
    [self release];
    return nil;
   }
   _data=[nibData copy];
   _bundle=[bundle retain];
   _allObjects=[NSMutableArray new];
   return self;
}

-(void)dealloc {
   [_bundle release];
   [_data release];
   [_allObjects release];
   [super dealloc];
}

-unarchiver:(NSKeyedUnarchiver *)unarchiver didDecodeObject:object {
   if(object!=nil)
    [_allObjects addObject:object];
   return object;
}

-(void)unarchiver:(NSKeyedUnarchiver *)unarchiver willReplaceObject:object withObject:replacement {
   if(object!=nil && replacement!=nil){
    NSUInteger index=[_allObjects indexOfObjectIdenticalTo:object];
    // Minkar: an object still being decoded is not in the list yet.
    if(index!=NSNotFound)
     [_allObjects replaceObjectAtIndex:index withObject:replacement];
   }
}

-(NSDictionary *)externalNameTable {
   return _nameTable;
}

-(BOOL)instantiateNibWithExternalNameTable:(NSDictionary *)nameTable {
    
    NIBDEBUG(@"instantiateNibWithExternalNameTable: %@", nameTable);
    
   NSAutoreleasePool *pool=[NSAutoreleasePool new];
   _nameTable=[nameTable retain];
    // Minkar: a binary nib (NIBArchive) or a keyed archive.
    NSKeyedUnarchiver *unarchiver=[NSNibArchiveUnarchiver isNibArchive:_data] ?
        [[[NSNibArchiveUnarchiver alloc] initForReadingWithData:_data] autorelease] :
        [[[NSKeyedUnarchiver alloc] initForReadingWithData:_data] autorelease];

    if(unarchiver==nil){
     NSLog(@"NSNib: the nib's data cannot be read");
     [pool release];
     return NO;
    }
    NSIBObjectData    *objectData;
    int                i,count;
    NSMenu            *menu;
    NSArray           *topLevelObjects;
    
    [unarchiver setDelegate:self];
    
    /*
    TO DO:
     - utf8 in the multinational panel
     - misaligned objects in boxes everywhere
    */
    [unarchiver setClass:[NSTableCornerView class] forClassName:@"_NSCornerView"];
    [unarchiver setClass:[NSNibHelpConnector class] forClassName:@"NSIBHelpConnector"];
    
    objectData=[unarchiver decodeObjectForKey:@"IB.objectdata"];
    [objectData buildConnectionsWithNameTable:_nameTable];
	if((menu=[objectData mainMenu])!=nil) {
		// Rename the first item to have the application name.
		if ([menu numberOfItems] > 0) {
			NSMenuItem *firstItem = [menu itemAtIndex: 0];
			NSString *appName = [NSString stringWithFormat:@"!%@", [[[NSBundle mainBundle] localizedInfoDictionary] objectForKey:(NSString *)kCFBundleNameKey]];
			[firstItem setTitle: appName];
			[[firstItem submenu] setValue:@"NSAppleMenu" forKey:@"name"];
		}
		[NSApp setMainMenu:menu];
	}
	
    topLevelObjects = [objectData topLevelObjects];

    // Top-level objects are always retained - this echoes observed Cocoa behaviour
	[topLevelObjects makeObjectsPerformSelector:@selector(retain)];

    // if external table contains a mutable array for key NSNibTopLevelObjects,
	// then this array also retains all top-level objects,
    if([_nameTable objectForKey:NSNibTopLevelObjects]) {
        [[_nameTable objectForKey:NSNibTopLevelObjects] setArray:topLevelObjects];
	}
    
    // We do not need to add the objects from nameTable to allObjects as they get put into the uid->object table already
    // Do we send awakeFromNib to objects in the nameTable *not* present in the nib ?

    count=[_allObjects count];

    for(i=0;i<count;i++){
     id object=[_allObjects objectAtIndex:i];
     
     if([object respondsToSelector:@selector(awakeFromNib)])
      [object awakeFromNib];
    }

    for(i=0;i<count;i++){
     id object=[_allObjects objectAtIndex:i];

     if([object respondsToSelector:@selector(postAwakeFromNib)])
      [object performSelector:@selector(postAwakeFromNib)];
    }

    [[objectData visibleWindows] makeObjectsPerformSelector:@selector(makeKeyAndOrderFront:) withObject:nil];
    
    [_nameTable release];
    _nameTable=nil;

    [pool release];

    return (objectData!=nil);
}

-(BOOL)instantiateNibWithOwner:owner topLevelObjects:(NSArray **)objects {

    NIBDEBUG(@"instantiateNibWithOwner: %@ topLevelObjects: ", owner);

    NSMutableArray   *topLevelObjects = (objects != NULL ? [[NSMutableArray alloc] init] : nil);
   NSMutableDictionary *nameTable=[NSMutableDictionary dictionary];

   /* A nil owner must not end the table before the top-level objects. */
   if(owner!=nil)
    [nameTable setObject:owner forKey:NSNibOwner];
   if(topLevelObjects!=nil)
    [nameTable setObject:topLevelObjects forKey:NSNibTopLevelObjects];
   BOOL             result = [self instantiateNibWithExternalNameTable:nameTable];
   
   if(objects != NULL){
       if(result)
           *objects = [NSArray arrayWithArray:topLevelObjects];
       [topLevelObjects release];
   }
   
   return result;
}

/* As the older method, but the caller does not own the top-level
   objects: they are released at the end of the autorelease pool, and
   kept by whoever needs them (the array given back holds them). */
-(BOOL)instantiateWithOwner:(id)owner topLevelObjects:(NSArray **)topLevelObjects {
   NSArray *objects=nil;
   BOOL     result=[self instantiateNibWithOwner:owner topLevelObjects:&objects];

   if(result)
    for(id object in objects)
     [object autorelease];
   if(topLevelObjects!=NULL)
    *topLevelObjects=result?objects:nil;
   return result;
}

@end
