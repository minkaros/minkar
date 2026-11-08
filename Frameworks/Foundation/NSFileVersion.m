/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSFileVersion. A document's earlier contents are files in
 * ~/Library/Versions/<key>/, the key made from the document's path; the
 * folder's versions.plist lists them (identifier, file, date, name) and
 * names the document they are of. The current version is the document
 * itself. There are no conflicts: no other computer saves here.
 */

#import <Foundation/NSFileVersion.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSError.h>
#import <Foundation/FoundationErrors.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSPropertyList.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSUUID.h>
#include <stdint.h>

static NSString *const kIndex=@"versions.plist";

/* ~/Library/Versions/<key>: the key a 64-bit FNV-1a hash of the path. */
static NSString *versionsFolderForItem(NSURL *url) {
   NSString   *path=[[[url URLByStandardizingPath] path] stringByResolvingSymlinksInPath];
   const char *bytes=[path fileSystemRepresentation];
   uint64_t    hash=1469598103934665603ULL;

   for(;bytes!=NULL && *bytes;bytes++){
    hash^=(unsigned char)*bytes;
    hash*=1099511628211ULL;
   }
   return [[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Versions"]
      stringByAppendingPathComponent:[NSString stringWithFormat:@"%016llx",(unsigned long long)hash]];
}

static NSMutableArray *readIndex(NSString *folder) {
   NSDictionary *index=[NSDictionary dictionaryWithContentsOfFile:[folder stringByAppendingPathComponent:kIndex]];
   NSArray      *versions=[index objectForKey:@"versions"];

   return [NSMutableArray arrayWithArray:[versions isKindOfClass:[NSArray class]]?versions:[NSArray array]];
}

static BOOL writeIndex(NSString *folder,NSURL *item,NSArray *versions) {
   NSDictionary *index=[NSDictionary dictionaryWithObjectsAndKeys:
      [[item URLByStandardizingPath] path],@"item",versions,@"versions",nil];

   return [index writeToFile:[folder stringByAppendingPathComponent:kIndex] atomically:YES];
}

static NSError *versionError(NSInteger code,NSURL *url) {
   return [NSError errorWithDomain:NSCocoaErrorDomain code:code
      userInfo:(url!=nil)?[NSDictionary dictionaryWithObject:url forKey:NSURLErrorKey]:nil];
}

@interface NSFileVersion ()
-(id)_initWithItemURL:(NSURL *)item URL:(NSURL *)url identifier:(NSString *)identifier
   name:(NSString *)name date:(NSDate *)date current:(BOOL)current;
@end

@implementation NSFileVersion

-(id)_initWithItemURL:(NSURL *)item URL:(NSURL *)url identifier:(NSString *)identifier
   name:(NSString *)name date:(NSDate *)date current:(BOOL)current {
   if((self=[super init])!=nil){
    _itemURL=[item copy];
    _URL=[url copy];
    _identifier=[identifier copy];
    _localizedName=[name copy];
    _modificationDate=[date copy];
    _isCurrent=current;
    _resolved=YES;
   }
   return self;
}

-(void)dealloc {
   [_itemURL release];
   [_URL release];
   [_identifier release];
   [_localizedName release];
   [_modificationDate release];
   [super dealloc];
}

+(NSFileVersion *)currentVersionOfItemAtURL:(NSURL *)url {
   NSDictionary *attributes=[[NSFileManager defaultManager] attributesOfItemAtPath:[url path] error:NULL];

   if(attributes==nil)
    return nil;
   return [[[self alloc] _initWithItemURL:url URL:url identifier:@"current"
      name:[[url path] lastPathComponent] date:[attributes fileModificationDate] current:YES] autorelease];
}

+(NSArray *)otherVersionsOfItemAtURL:(NSURL *)url {
   NSString       *folder=versionsFolderForItem(url);
   NSMutableArray *result=[NSMutableArray array];

   for(NSDictionary *entry in readIndex(folder)){
    NSString *file=[entry objectForKey:@"file"];
    NSString *path=[folder stringByAppendingPathComponent:file];

    if(![[NSFileManager defaultManager] fileExistsAtPath:path])
     continue;
    [result addObject:[[[self alloc] _initWithItemURL:url URL:[NSURL fileURLWithPath:path]
       identifier:[entry objectForKey:@"identifier"] name:[entry objectForKey:@"name"]
       date:[entry objectForKey:@"date"] current:NO] autorelease]];
   }
   return result;
}

+(NSArray *)unresolvedConflictVersionsOfItemAtURL:(NSURL *)url {
   return [NSArray array];
}

+(NSFileVersion *)versionOfItemAtURL:(NSURL *)url forPersistentIdentifier:(id)persistentIdentifier {
   if([persistentIdentifier isEqual:@"current"])
    return [self currentVersionOfItemAtURL:url];
   for(NSFileVersion *version in [self otherVersionsOfItemAtURL:url])
    if([version->_identifier isEqual:persistentIdentifier])
     return version;
   return nil;
}

+(NSFileVersion *)addVersionOfItemAtURL:(NSURL *)url withContentsOfURL:(NSURL *)contentsURL
   options:(NSFileVersionAddingOptions)options error:(NSError **)outError {
   NSFileManager  *manager=[NSFileManager defaultManager];
   NSString       *folder=versionsFolderForItem(url);
   NSString       *identifier=[[NSUUID UUID] UUIDString];
   NSString       *extension=[[url path] pathExtension];
   NSString       *file=([extension length]>0)?[identifier stringByAppendingPathExtension:extension]:identifier;
   NSString       *path=[folder stringByAppendingPathComponent:file];
   NSDictionary   *attributes=[manager attributesOfItemAtPath:[contentsURL path] error:NULL];
   NSDate         *date=([attributes fileModificationDate]!=nil)?[attributes fileModificationDate]:[NSDate date];
   NSMutableArray *index;
   NSError        *error=nil;
   BOOL            done;

   if(![manager createDirectoryAtPath:folder withIntermediateDirectories:YES
      attributes:[NSDictionary dictionaryWithObject:[NSNumber numberWithShort:0700] forKey:NSFilePosixPermissions]
      error:&error]){
    if(outError!=NULL)
     *outError=error;
    return nil;
   }
   if(options&NSFileVersionAddingByMoving)
    done=[manager moveItemAtPath:[contentsURL path] toPath:path error:&error];
   else
    done=[manager copyItemAtPath:[contentsURL path] toPath:path error:&error];
   if(!done){
    if(outError!=NULL)
     *outError=error;
    return nil;
   }

   index=readIndex(folder);
   [index addObject:[NSDictionary dictionaryWithObjectsAndKeys:identifier,@"identifier",file,@"file",
      date,@"date",[[url path] lastPathComponent],@"name",nil]];
   if(!writeIndex(folder,url,index)){
    [manager removeItemAtPath:path error:NULL];
    if(outError!=NULL)
     *outError=versionError(NSFileWriteUnknownError,url);
    return nil;
   }
   return [[[self alloc] _initWithItemURL:url URL:[NSURL fileURLWithPath:path] identifier:identifier
      name:[[url path] lastPathComponent] date:date current:NO] autorelease];
}

+(NSURL *)temporaryDirectoryURLForNewVersionOfItemAtURL:(NSURL *)url {
   NSString *path=[NSTemporaryDirectory() stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]];

   [[NSFileManager defaultManager] createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:NULL];
   return [NSURL fileURLWithPath:path isDirectory:YES];
}

+(BOOL)removeOtherVersionsOfItemAtURL:(NSURL *)url error:(NSError **)outError {
   NSString *folder=versionsFolderForItem(url);
   NSError  *error=nil;

   if(![[NSFileManager defaultManager] fileExistsAtPath:folder])
    return YES;
   if(![[NSFileManager defaultManager] removeItemAtPath:folder error:&error]){
    if(outError!=NULL)
     *outError=error;
    return NO;
   }
   return YES;
}

-(NSURL *)URL {
   return _URL;
}

-(NSString *)localizedName {
   return _localizedName;
}

-(NSString *)localizedNameOfSavingComputer {
   return nil;
}

-(NSDate *)modificationDate {
   return _modificationDate;
}

-(id<NSCoding>)persistentIdentifier {
   return _identifier;
}

-(BOOL)isConflict {
   return NO;
}

-(BOOL)isResolved {
   return _resolved;
}

-(void)setResolved:(BOOL)value {
   _resolved=value;
}

-(BOOL)isDiscardable {
   return _discardable;
}

-(void)setDiscardable:(BOOL)value {
   _discardable=value;
}

-(BOOL)hasLocalContents {
   return YES;
}

-(BOOL)hasThumbnail {
   return NO;
}

/* Puts this version's contents at url (the document's place, usually). */
-(NSURL *)replaceItemAtURL:(NSURL *)url options:(NSFileVersionReplacingOptions)options error:(NSError **)error {
   NSFileManager *manager=[NSFileManager defaultManager];
   NSString      *temporary;
   NSError       *failure=nil;

   if(_isCurrent && [[_URL path] isEqualToString:[url path]])
    return url;

   temporary=[[[url path] stringByDeletingLastPathComponent] stringByAppendingPathComponent:
      [NSString stringWithFormat:@".%@.%@",[[NSUUID UUID] UUIDString],[[url path] lastPathComponent]]];
   if(![manager copyItemAtPath:[_URL path] toPath:temporary error:&failure]){
    if(error!=NULL)
     *error=failure;
    return nil;
   }
   if([manager fileExistsAtPath:[url path]] && ![manager removeItemAtPath:[url path] error:&failure]){
    [manager removeItemAtPath:temporary error:NULL];
    if(error!=NULL)
     *error=failure;
    return nil;
   }
   if(![manager moveItemAtPath:temporary toPath:[url path] error:&failure]){
    if(error!=NULL)
     *error=failure;
    return nil;
   }
   if(options&NSFileVersionReplacingByMoving)
    [self removeAndReturnError:NULL];
   return url;
}

-(BOOL)removeAndReturnError:(NSError **)outError {
   NSString       *folder;
   NSMutableArray *index;
   NSUInteger      i;

   if(_isCurrent){
    if(outError!=NULL)
     *outError=versionError(NSFileWriteNoPermissionError,_itemURL);
    return NO;
   }
   folder=versionsFolderForItem(_itemURL);
   index=readIndex(folder);
   for(i=0;i<[index count];i++)
    if([[[index objectAtIndex:i] objectForKey:@"identifier"] isEqual:_identifier]){
     [index removeObjectAtIndex:i];
     break;
    }
   [[NSFileManager defaultManager] removeItemAtPath:[_URL path] error:NULL];
   return writeIndex(folder,_itemURL,index);
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@: %p %@ %@>",[self class],self,_localizedName,_modificationDate];
}

@end
