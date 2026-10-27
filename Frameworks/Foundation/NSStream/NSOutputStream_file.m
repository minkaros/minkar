/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// Original - Christopher Lloyd <cjwl@objc.net>
#import <Foundation/NSOutputStream_file.h>
#import <Foundation/NSFileHandle.h>
#import <Foundation/NSString.h>
#import <Foundation/NSError.h>
#import <Foundation/NSData.h>
#import <Foundation/NSPathUtilities.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

@implementation NSOutputStream_file

-initToFileAtPath:(NSString *)path append:(BOOL)append {
   _delegate=self;
   _error=nil;
   _status=NSStreamStatusNotOpen;
   _path=[path copy];
   _append=append;
   _fileHandle=nil;
   return self;
}

-(void)dealloc {
   [_error release];
   [_path release];
   [_fileHandle release];
   [super dealloc];
}

-delegate {
   return _delegate;
}

-(void)setDelegate:delegate {
   _delegate=delegate;
   if(_delegate==nil)
    _delegate=self;
}

-(NSError *)streamError {
   return _error;
}

-(NSStreamStatus)streamStatus {
   return _status;
}

// Minkar: the file is made if it does not exist (fileHandleForWritingAtPath:
// opens only existing files, so nothing was written) and emptied unless
// appending.
-(void)open {
   if(_status==NSStreamStatusNotOpen){
    int fd=open([_path fileSystemRepresentation],
                O_WRONLY|O_CREAT|(_append?O_APPEND:O_TRUNC),0666);
    if(fd<0){
     [self failWithErrno:errno];
     return;
    }
    _fileHandle=[[NSFileHandle alloc] initWithFileDescriptor:fd closeOnDealloc:YES];
    _status=NSStreamStatusOpen;
   }
}

-(void)failWithErrno:(int)code {
   [_error release];
   _error=[[NSError alloc] initWithDomain:NSPOSIXErrorDomain code:code userInfo:nil];
   _status=NSStreamStatusError;
}

-(void)close {
   _status=NSStreamStatusClosed;
   [_fileHandle closeFile];
}

-propertyForKey:(NSString *)key {
   return nil;
}

-(BOOL)hasSpaceAvailable {
   return (_status==NSStreamStatusOpen || _status==NSStreamStatusWriting)?YES:NO;
}

// Bytes written, or -1 on an error (streamError says which) or when the
// stream is not open.
-(NSInteger)write:(const uint8_t *)buffer maxLength:(NSUInteger)maxLength {
   ssize_t n;

   if(_status!=NSStreamStatusOpen && _status!=NSStreamStatusWriting)
    return -1;
   _status=NSStreamStatusWriting;
   do {
    n=write([_fileHandle fileDescriptor],buffer,maxLength);
   } while(n<0 && errno==EINTR);
   if(n<0){
    [self failWithErrno:errno];
    return -1;
   }
   _status=NSStreamStatusOpen;
   return n;
}

@end
