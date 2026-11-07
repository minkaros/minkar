/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#ifdef PLATFORM_IS_POSIX
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSThread.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSThread-Private.h>
#import <Foundation/NSRaiseException.h>
#import <Foundation/NSPlatform_posix.h>

#import "../NSString/NSString_cString.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <string.h>
#include <sys/mount.h>
#include <dirent.h>
#import <Foundation/NSError.h>
#import <Foundation/FoundationErrors.h>
#import <objc/runtime.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

static NSError *fileError(int code, NSString *path, BOOL writing);

/* Minkar: the file system operations are NSFileManager's own (a
 * category), so that NSFileManager is concrete and can be subclassed, as
 * on the reference platform: a Swift subclass of FileManager was given
 * NSFileManager_posix instead, and super reached empty methods. */
@implementation NSFileManager (posix)

-(BOOL)createFileAtPath:(NSString *)path contents:(NSData *)data
             attributes:(NSDictionary *)attributes {
    return [[NSPlatform currentPlatform] writeContentsOfFile:path bytes:[data bytes] length:[data length] options:NSAtomicWrite error:NULL];
}

-(NSArray *)directoryContentsAtPath:(NSString *)path {
    return [self contentsOfDirectoryAtPath:path error:NULL];
}

-(NSArray *)contentsOfDirectoryAtPath:(NSString *)path error:(NSError **)error
{
//TODO fill error
    NSMutableArray *result=nil;
    DIR *dirp = NULL;
    struct dirent *dire;

    if(path == nil) {
        return nil;
    }

    dirp = opendir([path fileSystemRepresentation]);
    
    if (dirp == NULL)
        return nil;

    result=[NSMutableArray array];

    while ((dire = readdir(dirp))){
	 if(strcmp(".",dire->d_name)==0)
	  continue;
	 if(strcmp("..",dire->d_name)==0)
	  continue;
     [result addObject:[NSString stringWithCString:dire->d_name]];
    }

    closedir(dirp);
    
    return result;
}


-(BOOL)createDirectoryAtPath:(NSString *)path attributes:(NSDictionary *)attributes {
    // you can set all these, but we don't respect 'em all yet
    //NSDate *date = [attributes objectForKey:NSFileModificationDate];
    //NSString *owner = [attributes objectForKey:NSFileOwnerAccountName];
    //NSString *group = [attributes objectForKey:NSFileGroupOwnerAccountName];
    int mode = [[attributes objectForKey:NSFilePosixPermissions] intValue];

    if (mode == 0)
        mode = FOUNDATION_DIR_MODE;

    return (mkdir([path fileSystemRepresentation], mode) == 0);
}

-(BOOL)fileExistsAtPath:(NSString *)path isDirectory:(BOOL *)isDirectory {
    struct stat buf;

    if(stat([path fileSystemRepresentation],&buf)<0)
        return NO;

    if(isDirectory!=NULL)
     *isDirectory=S_ISDIR(buf.st_mode);

    return YES;
}

// we dont want to use fileExists... because it chases links
-(BOOL)_isDirectory:(NSString *)path {
    struct stat buf;

    if(lstat([path fileSystemRepresentation],&buf)<0)
        return NO;

    if (buf.st_mode & S_IFDIR && !(buf.st_mode & S_IFLNK))
        return YES;

    return NO;
}

-(BOOL)_errorHandler:handler src:(NSString *)src dest:(NSString *)dest operation:(NSString *)op {
    if ([handler respondsToSelector:@selector(fileManager:shouldProceedAfterError:)]) {
        NSDictionary *errorInfo = [NSDictionary dictionaryWithObjectsAndKeys:
            src, @"Path",
            [NSString stringWithFormat:@"%@: %s", op, strerror(errno)], @"Error",
            dest, @"ToPath",
            nil];

        if ([handler fileManager:self shouldProceedAfterError:errorInfo])
            return YES;
    }

    return NO;
}

- (BOOL)removeItemAtPath:(NSString *)path error:(NSError **)error {
    if([path isEqualToString:@"."] || [path isEqualToString:@".."])
        NSRaiseException(NSInvalidArgumentException, self, _cmd, @"%@: invalid path", path);

    if(![self _isDirectory:path]){
        if(remove([path fileSystemRepresentation]) == -1) {
            if(error!=NULL)
				*error=nil; //TODO set error
            return NO;
        }
    }
    else{
        NSArray *contents=[self directoryContentsAtPath:path];
        NSInteger i,count=[contents count];
        
        for(i=0;i<count;i++){
            NSString *name = [contents objectAtIndex:i];
            NSString *fullPath;
            
            if([name isEqualToString:@"."] || [name isEqualToString:@".."])
                continue;
            
            fullPath=[path stringByAppendingPathComponent:name];
            if(![self removeItemAtPath:fullPath error:error]) {
                if(error!=NULL)
                    *error=nil; //TODO set error
                return NO;
            }
        }
        
        if(rmdir([path fileSystemRepresentation]) == -1) {
            if(error!=NULL)
				*error=nil; //TODO set error
            return NO;
        }
    }
    return YES;
}

-(BOOL)removeFileAtPath:(NSString *)path handler:handler {
    NSError *error = nil;
    
    if ([handler respondsToSelector:@selector(fileManager:willProcessPath:)])
        [handler fileManager:self willProcessPath:path];

    if ([self removeItemAtPath:path error:&error] == NO && handler != nil) {
        [self _errorHandler:handler src:path dest:@"" operation:[error description]];
        return NO;
    }
    
    return YES;
}


-(BOOL)movePath:(NSString *)src toPath:(NSString *)dest handler:handler {
    NSError *error = nil;
    
    if ([handler respondsToSelector:@selector(fileManager:willProcessPath:)])
        [handler fileManager:self willProcessPath:src];

    if ([self moveItemAtPath:src toPath:dest error:&error] == NO && handler != nil) {
        [self _errorHandler:handler src:src dest:dest operation:[error description]];
        return NO;
    }
    
    return YES;
}

- (BOOL)moveItemAtPath:(NSString *)srcPath toPath:(NSString *)dstPath error:(NSError **)error
{
    
    /*
     It's not this easy...
     return rename([src fileSystemRepresentation],[dest fileSystemRepresentation])?NO:YES;
     */

    BOOL isDirectory;

    if ([self fileExistsAtPath:srcPath isDirectory:&isDirectory] == NO) {
        if (error != NULL)
            *error = fileError(ENOENT, srcPath, NO);
        return NO;
    }
    if ([self fileExistsAtPath:dstPath isDirectory:&isDirectory] == YES) {
        if (error != NULL)
            *error = fileError(EEXIST, dstPath, YES);
        return NO;
    }
    /* On the same volume, rename(2) (atomic, keeps the item); across
       volumes, a copy then the original removed. */
    if (rename([srcPath fileSystemRepresentation], [dstPath fileSystemRepresentation]) == 0)
        return YES;
    if (errno != EXDEV) {
        if (error != NULL)
            *error = fileError(errno, srcPath, YES);
        return NO;
    }
    
    if ([self copyPath:srcPath toPath:dstPath handler:nil] == NO) {
        [self removeFileAtPath:dstPath handler:nil];
        return NO;
    }
    
    // not much we can do if this fails
    [self removeFileAtPath:srcPath handler:nil];
    
    return YES;
}

-(BOOL)copyPath:(NSString *)src toPath:(NSString *)dest handler:handler {
    NSError *error = nil;
    if ([self copyItemAtPath:src toPath:dest error:&error] == NO && handler != nil) {
        [self _errorHandler:handler src:src dest:dest operation:[error description]];
        return NO;
    }
    
    return YES;
}

-(BOOL)copyItemAtPath:(NSString *)fromPath toPath:(NSString *)toPath error:(NSError **)error
{
    BOOL isDirectory;
    
    if(![self fileExistsAtPath:fromPath isDirectory:&isDirectory]) {
        if (error != NULL) {
            //TODO set error
        }
        return NO;
    }    
    
    if (!isDirectory){
        int r, w;
        char buf[4096];
        size_t count;
        
        if ((w = open([toPath fileSystemRepresentation], O_WRONLY|O_CREAT, FOUNDATION_FILE_MODE)) == -1) {
            if (error != NULL) {
                //TODO set error
            }
            return NO;
        }
        if ((r = open([fromPath fileSystemRepresentation], O_RDONLY)) == -1) {
            if (error != NULL) {
                //TODO set error
            }
            close(w);
            return NO;

        }
        
        while ((count = read(r, &buf, sizeof(buf))) > 0) {
            if (count == -1) 
                break;
            
            if (write(w, &buf, count) != count) {
                count = -1;
                break;
            }
        }
        
        close(w);
        close(r);
        
        if (count == -1) {
            if (error != NULL) {
                //TODO set error
            }
            return NO;
        }
        else
            return YES;
    }
    else {
        NSArray *files;
        NSInteger      i,count;
        
        if (mkdir([toPath fileSystemRepresentation], FOUNDATION_DIR_MODE) != 0) {
            if (error != NULL) {
                //TODO set error
            }
            return NO;
        }
        
        //if (chdir([dest fileSystemRepresentation]) != 0)
        //    return [self _errorHandler:handler src:src dest:dest operation:@"copyPath: chdir(subdir)"];
        
        files = [self directoryContentsAtPath:fromPath];
        count = [files count];
        
        for(i=0;i<count;i++){
            NSString *name=[files objectAtIndex:i];
            NSString *subsrc, *subdst;
            
            if ([name isEqualToString:@"."] || [name isEqualToString:@".."])
                continue;
            
            subsrc=[fromPath stringByAppendingPathComponent:name];
            subdst=[toPath stringByAppendingPathComponent:name];
            
            if([self copyItemAtPath:subsrc toPath:subdst error:error] == NO) {
                if (error != NULL) {
                    //TODO set error
                }
                return NO;
            }
        }
        
        //if (chdir("..") != 0)
        //    return [self _errorHandler:handler src:src dest:dest operation:@"copyPath: chdir(..)"];
    }
    
    return YES;

}
-(NSString *)currentDirectoryPath {
    char  path[MAXPATHLEN+1];

    if (getcwd(path, sizeof(path)) != NULL)
        return [NSString stringWithCString:path];

    return nil;
}

-(NSString *)pathContentOfSymbolicLinkAtPath:(NSString *)path {
    char linkbuf[MAXPATHLEN+1];
    size_t length;

    length = readlink([path fileSystemRepresentation], linkbuf, MAXPATHLEN);
    if (length ==-1)
        return nil;

    linkbuf[length] = 0;
    return [NSString stringWithCString:linkbuf];
}


-(NSDictionary *)fileAttributesAtPath:(NSString *)path traverseLink:(BOOL)traverse {
    NSMutableDictionary *result=[NSMutableDictionary dictionary];
    struct stat statBuf;
    struct passwd *pwd;
    struct group *grp;

    if (lstat([path fileSystemRepresentation], &statBuf) != 0)
        return nil;

    // (Not in POSIX.1-1996.)
    if (S_ISLNK(statBuf.st_mode) && traverse) {
        NSString *linkContents = [self pathContentOfSymbolicLinkAtPath:path];
        return [self fileAttributesAtPath:linkContents traverseLink:traverse];
    }

    [result setObject:[NSNumber numberWithUnsignedLong:statBuf.st_size]
               forKey:NSFileSize];
    [result setObject:[NSDate dateWithTimeIntervalSince1970:statBuf.st_mtimespec.tv_sec +
                           statBuf.st_mtimespec.tv_nsec / 1e9]
               forKey:NSFileModificationDate];
    /* Darwin keeps the creation (birth) time. */
    [result setObject:[NSDate dateWithTimeIntervalSince1970:statBuf.st_birthtimespec.tv_sec +
                           statBuf.st_birthtimespec.tv_nsec / 1e9]
               forKey:NSFileCreationDate];
    [result setObject:[NSNumber numberWithUnsignedInt:statBuf.st_uid] forKey:NSFileOwnerAccountID];
    [result setObject:[NSNumber numberWithUnsignedInt:statBuf.st_gid] forKey:NSFileGroupOwnerAccountID];
    [result setObject:[NSNumber numberWithBool:(statBuf.st_flags & (UF_IMMUTABLE | SF_IMMUTABLE)) != 0]
               forKey:NSFileImmutable];
    [result setObject:[NSNumber numberWithBool:(statBuf.st_flags & (UF_APPEND | SF_APPEND)) != 0]
               forKey:NSFileAppendOnly];
    [result setObject:[NSNumber numberWithBool:NO] forKey:NSFileExtensionHidden];

    // User/group names don't always exist for the IDs in the filesystem.
    // If we don't check for NULLs, we'll segfault.
    pwd = getpwuid(statBuf.st_uid);
    if (pwd != NULL)
        [result setObject:[NSString stringWithCString:pwd->pw_name]
                   forKey:NSFileOwnerAccountName];

    grp = getgrgid(statBuf.st_gid);
    if (grp != NULL)
        [result setObject:[NSString stringWithCString:grp->gr_name]
                   forKey:NSFileGroupOwnerAccountName];

    [result setObject:[NSNumber numberWithUnsignedLong:statBuf.st_nlink]
               forKey:NSFileReferenceCount];
    [result setObject:[NSNumber numberWithUnsignedLong:statBuf.st_ino]
               forKey:NSFileIdentifier];
    [result setObject:[NSNumber numberWithUnsignedLong:statBuf.st_dev]
               forKey:NSFileDeviceIdentifier];
    [result setObject:[NSNumber numberWithInt:statBuf.st_dev] forKey:NSFileSystemNumber];
    /* The permission bits alone, as on the reference platform (it was
       the whole mode, type bits too). */
    [result setObject:[NSNumber numberWithUnsignedLong:statBuf.st_mode & 07777]
               forKey:NSFilePosixPermissions];
    [result setObject:[NSNumber numberWithUnsignedLongLong:statBuf.st_ino]
               forKey:NSFileSystemFileNumber];

    // ugh.. skip this if we can
    if (!S_ISREG(statBuf.st_mode)) {
        if (S_ISDIR(statBuf.st_mode))
            [result setObject:NSFileTypeDirectory forKey:NSFileType];
        else if (S_ISCHR(statBuf.st_mode))
            [result setObject:NSFileTypeCharacterSpecial forKey:NSFileType];
        else if (S_ISBLK(statBuf.st_mode))
            [result setObject:NSFileTypeBlockSpecial forKey:NSFileType];
        else if (S_ISFIFO(statBuf.st_mode))
            [result setObject:NSFileTypeFIFO forKey:NSFileType];
        else if (S_ISLNK(statBuf.st_mode))
            [result setObject:NSFileTypeSymbolicLink forKey:NSFileType];
        else if (S_ISSOCK(statBuf.st_mode))
            [result setObject:NSFileTypeSocket forKey:NSFileType];
    }
    else
        [result setObject:NSFileTypeRegular forKey:NSFileType];

    return result;
}

/* ---- The path-based API of the reference platform (API 19) ----
 * Cocoa errors from errno, with the path and the POSIX error beneath. */

static NSError *fileError(int code, NSString *path, BOOL writing) {
   NSInteger cocoa;

   switch(code) {
   case ENOENT: cocoa=NSFileNoSuchFileError; break;
   case EEXIST: cocoa=NSFileWriteFileExistsError; break;
   case EACCES: case EPERM: cocoa=writing ? NSFileWriteNoPermissionError : NSFileReadNoPermissionError; break;
   default: cocoa=writing ? NSFileWriteUnknownError : NSFileReadUnknownError; break;
   }
   NSError *posix=[NSError errorWithDomain:NSPOSIXErrorDomain code:code userInfo:nil];
   NSMutableDictionary *info=[NSMutableDictionary dictionaryWithObject:posix forKey:NSUnderlyingErrorKey];
   if(path!=nil)
      [info setObject:path forKey:NSFilePathErrorKey];
   return [NSError errorWithDomain:NSCocoaErrorDomain code:cocoa userInfo:info];
}

/* The item itself: a symbolic link's attributes, not its destination's. */
-(NSDictionary *)attributesOfItemAtPath:(NSString *)path error:(NSError **)error {
   struct stat st;

   if(path==nil || lstat([path fileSystemRepresentation],&st)!=0) {
      if(error!=NULL)
         *error=fileError(path ? errno : ENOENT,path,NO);
      return nil;
   }
   return [self fileAttributesAtPath:path traverseLink:NO];
}

-(NSDictionary *)attributesOfFileSystemForPath:(NSString *)path error:(NSError **)error {
   struct statfs fs;

   if(path==nil || statfs([path fileSystemRepresentation],&fs)!=0) {
      if(error!=NULL)
         *error=fileError(path ? errno : ENOENT,path,NO);
      return nil;
   }
   return [NSDictionary dictionaryWithObjectsAndKeys:
      [NSNumber numberWithUnsignedLongLong:(unsigned long long)fs.f_blocks*fs.f_bsize],NSFileSystemSize,
      [NSNumber numberWithUnsignedLongLong:(unsigned long long)fs.f_bavail*fs.f_bsize],NSFileSystemFreeSize,
      [NSNumber numberWithUnsignedLongLong:fs.f_files],@"NSFileSystemNodes",
      [NSNumber numberWithUnsignedLongLong:fs.f_ffree],@"NSFileSystemFreeNodes",
      [NSNumber numberWithInt:fs.f_fsid.val[0]],NSFileSystemNumber,nil];
}

-(BOOL)changeCurrentDirectoryPath:(NSString *)path {
   return path!=nil && chdir([path fileSystemRepresentation])==0;
}

-(BOOL)createSymbolicLinkAtPath:(NSString *)path withDestinationPath:(NSString *)destination error:(NSError **)error {
   if(symlink([destination fileSystemRepresentation],[path fileSystemRepresentation])!=0) {
      if(error!=NULL)
         *error=fileError(errno,path,YES);
      return NO;
   }
   return YES;
}

-(BOOL)linkItemAtPath:(NSString *)fromPath toPath:(NSString *)toPath error:(NSError **)error {
   if(link([fromPath fileSystemRepresentation],[toPath fileSystemRepresentation])!=0) {
      if(error!=NULL)
         *error=fileError(errno,toPath,YES);
      return NO;
   }
   return YES;
}

-(BOOL)linkPath:(NSString *)source toPath:(NSString *)destination handler:handler {
   NSError *error=nil;

   if(![self linkItemAtPath:source toPath:destination error:&error]) {
      if(handler!=nil)
         [self _errorHandler:handler src:source dest:destination operation:[error description]];
      return NO;
   }
   return YES;
}

/* Deletable: its directory is writable (and, sticky, the item is ours). */
-(BOOL)isDeletableFileAtPath:(NSString *)path {
   NSString *parent=[path stringByDeletingLastPathComponent];
   struct stat dir,item;

   if([parent length]==0)
      parent=@".";
   if(access([parent fileSystemRepresentation],W_OK|X_OK)!=0)
      return NO;
   if(stat([parent fileSystemRepresentation],&dir)==0 && (dir.st_mode&S_ISVTX) &&
      lstat([path fileSystemRepresentation],&item)==0 && geteuid()!=0 &&
      item.st_uid!=geteuid() && dir.st_uid!=geteuid())
      return NO;
   return YES;
}

-(BOOL)contentsEqualAtPath:(NSString *)path1 andPath:(NSString *)path2 {
   BOOL dir1,dir2;

   if(![self fileExistsAtPath:path1 isDirectory:&dir1] || ![self fileExistsAtPath:path2 isDirectory:&dir2] || dir1!=dir2)
      return NO;
   if(!dir1)
      return [[NSData dataWithContentsOfFile:path1] isEqual:[NSData dataWithContentsOfFile:path2]];
   NSArray *a=[[self contentsOfDirectoryAtPath:path1 error:NULL] sortedArrayUsingSelector:@selector(compare:)];
   NSArray *b=[[self contentsOfDirectoryAtPath:path2 error:NULL] sortedArrayUsingSelector:@selector(compare:)];
   if(![a isEqual:b])
      return NO;
   for(NSString *name in a)
      if(![self contentsEqualAtPath:[path1 stringByAppendingPathComponent:name]
                            andPath:[path2 stringByAppendingPathComponent:name]])
         return NO;
   return YES;
}

-(NSArray *)subpathsOfDirectoryAtPath:(NSString *)path error:(NSError **)error {
   BOOL isDirectory;

   if(![self fileExistsAtPath:path isDirectory:&isDirectory] || !isDirectory) {
      if(error!=NULL)
         *error=fileError(ENOENT,path,NO);
      return nil;
   }
   return [self subpathsAtPath:path];
}

-(NSArray *)componentsToDisplayForPath:(NSString *)path {
   NSMutableArray *result=[NSMutableArray array];
   NSString *partial=@"/";

   if(![path isAbsolutePath] || ![self fileExistsAtPath:path])
      return nil;
   [result addObject:[self displayNameAtPath:@"/"]];
   for(NSString *component in [path pathComponents]) {
      if([component isEqualToString:@"/"])
         continue;
      partial=[partial stringByAppendingPathComponent:component];
      [result addObject:[self displayNameAtPath:partial]];
   }
   return result;
}

static char delegateKey;

-delegate {
   return objc_getAssociatedObject(self,&delegateKey);
}

-(void)setDelegate:delegate {
   objc_setAssociatedObject(self,&delegateKey,delegate,OBJC_ASSOCIATION_ASSIGN);
}

-(BOOL)isReadableFileAtPath:(NSString *)path {
    return access([path fileSystemRepresentation], R_OK) ? NO : YES;
}

-(BOOL)isWritableFileAtPath:(NSString *)path {
    return access([path fileSystemRepresentation], W_OK) ? NO : YES;
}

-(BOOL)isExecutableFileAtPath:(NSString *)path {
    return access([path fileSystemRepresentation], X_OK) ? NO : YES;
}

-(BOOL)createSymbolicLinkAtPath:(NSString *)path pathContent:(NSString *)otherPath {
    return (symlink([otherPath fileSystemRepresentation], [path fileSystemRepresentation]) == 0);
}

-(BOOL)setAttributes:(NSDictionary *)attributes ofItemAtPath:(NSString *)path error:(NSError **)error
{
    if (error != NULL) {
        //TODO set error
    }
    
    return NO;
}

-(const char *)fileSystemRepresentationWithPath:(NSString *)path {
    return [path cStringUsingEncoding:NSUTF8StringEncoding];
}

-(NSString *)destinationOfSymbolicLinkAtPath:(NSString *)path error:(NSError **)error {
    char destination[MAXPATHLEN+1];
    ssize_t bytes;

    bytes = readlink([path fileSystemRepresentation], destination, MAXPATHLEN);

    if (bytes == -1) {
        //TODO fill error
        return nil;
    }

    destination[bytes] = 0;

    return [NSString stringWithCString:destination encoding:NSUTF8StringEncoding];
}

@end
#endif
