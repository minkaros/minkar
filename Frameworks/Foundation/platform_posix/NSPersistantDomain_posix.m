/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#ifdef PLATFORM_IS_POSIX
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSPropertyListReader.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSException.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSSet.h>
#import "NSPersistantDomain_posix.h"
#import <Foundation/NSPropertyList.h>
#import <Foundation/NSData.h>
#import <Foundation/NSException.h>
#import <Foundation/NSAutoreleasePool.h>
#include <dispatch/dispatch.h>

@implementation NSPersistantDomain_posix

-initWithName:(NSString *)name {
    NSDictionary *serializedDictionary;
    
    _path = [NSHomeDirectory() stringByAppendingPathComponent:@"Library"];
    _path = [_path stringByAppendingPathComponent:@"Preferences"];
    _path = [_path stringByAppendingPathComponent:name];
    _path = [[_path stringByAppendingPathExtension:@"plist"] retain];
    
    serializedDictionary = [NSPropertyListReader dictionaryWithContentsOfFile:_path];
    if (serializedDictionary != nil)
        _mutableDomain = [serializedDictionary mutableCopy];
    else
        _mutableDomain = [[NSMutableDictionary alloc] init];
    _changedKeys = [[NSMutableSet alloc] init];

    return self;
}

-(void)dealloc {
    [_mutableDomain release];
    [_changedKeys release];
    [_path release];
    
    [super dealloc];
}

+(NSPersistantDomain_posix *)persistantDomainWithName:(NSString *)name {
    return [[[self allocWithZone:NULL] initWithName:name] autorelease];
}

-(NSArray *)allKeys {
    @synchronized(self) {
        return [_mutableDomain allKeys];
    }
}

-(NSEnumerator *)keyEnumerator {
    return [[self allKeys] objectEnumerator];
}

-objectForKey:(NSString *)key {
    @synchronized(self) {
        return [[[_mutableDomain objectForKey:key] retain] autorelease];
    }
}

/* A change reaches the file soon after it is made, on a queue of its own,
   as the reference's preferences daemon has it at once: a process that
   ends without synchronizing (killed, or one that never calls it, as the
   API no longer asks) keeps it. Changes made together are written once. */
static dispatch_queue_t writeQueue(void) {
    static dispatch_queue_t queue;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        queue = dispatch_queue_create("org.minkar.Foundation.preferences", DISPATCH_QUEUE_SERIAL);
    });
    return queue;
}

-(void)_scheduleWrite {
    @synchronized(self) {
        if (_writeScheduled)
            return;
        _writeScheduled = YES;
    }
    [self retain];
    dispatch_async(writeQueue(), ^{
        NSAutoreleasePool *pool = [NSAutoreleasePool new];

        @try {
            [self synchronize];
        } @catch (id exception) {
            /* The write is tried again with the next change, or at a
               synchronize, which reports it. */
        }
        [pool release];
        [self release];
    });
}

-(void)setObject:object forKey:(NSString *)key {
    @synchronized(self) {
        [_mutableDomain setObject:object forKey:key];
        [_changedKeys addObject:key];
    }
    [self _scheduleWrite];
}

-(void)removeObjectForKey:(NSString *)key {
    @synchronized(self) {
        [_mutableDomain removeObjectForKey:key];
        [_changedKeys addObject:key];
    }
    [self _scheduleWrite];
}

-(void)createUserDefaultsDirectoryIfNeeded {
   // Leave the error checking up to -synchronize.
   [[NSFileManager defaultManager] createDirectoryAtPath:[_path stringByDeletingLastPathComponent]
                             withIntermediateDirectories:YES attributes:nil error:NULL];
}

/* As on the reference platform, both ways: what other processes wrote
   (defaults(1), a preference pane) is read, and the keys this process
   set or removed since the last time are written over it. It used to
   write this process's copy alone, losing the others' changes. */
-(void)synchronize {
  @synchronized(self) {
    _writeScheduled = NO;
    NSDictionary *onDisk = [NSPropertyListReader dictionaryWithContentsOfFile:_path];
    NSMutableDictionary *merged = onDisk ? [onDisk mutableCopy] : [[NSMutableDictionary alloc] init];

    for (NSString *key in _changedKeys) {
        id value = [_mutableDomain objectForKey:key];
        if (value != nil)
            [merged setObject:value forKey:key];
        else
            [merged removeObjectForKey:key];
    }
    if ([_changedKeys count] > 0) {
        [self createUserDefaultsDirectoryIfNeeded];
        /* XML: the old text format wrote numbers, booleans and data as
           strings, and a file with data in it did not read back. */
        NSData *xml = [NSPropertyListSerialization dataWithPropertyList:merged format:NSPropertyListXMLFormat_v1_0
                                                                options:0 error:NULL];
        if (xml == nil || [xml writeToFile:_path atomically:YES] == NO) {
            /* Not written (a folder not the user's): said, and tried again
               next time, as the reference's synchronize returns NO; an
               exception here took down the app (the Dock). */
            [merged release];
            NSLog(@"Cannot synchronize NSUserDefaults to %@", _path);
            return;
        }
        [_changedKeys removeAllObjects];
    }
    [_mutableDomain release];
    _mutableDomain = merged;
  }
}

@end
#endif
