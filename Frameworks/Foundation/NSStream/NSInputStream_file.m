/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// Original - Christopher Lloyd <cjwl@objc.net>
#import <Foundation/NSInputStream_file.h>
#import <Foundation/NSString.h>
#import <Foundation/NSError.h>
#import <Foundation/NSFileHandle.h>

#include <errno.h>
#include <unistd.h>

@implementation NSInputStream_file

-initWithFileAtPath:(NSString *)path {
   _delegate=self;
   _error=nil;
   _status=NSStreamStatusNotOpen;
   _path=[path copy];
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

-(void)open {
   if(_status==NSStreamStatusNotOpen){
    _fileHandle=[[NSFileHandle fileHandleForReadingAtPath:_path] retain];
    if(_fileHandle==nil)
     [self failWithErrno:errno];
    else
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

-(BOOL)getBuffer:(uint8_t **)buffer length:(NSUInteger *)length {
   return NO;
}

// Minkar: these returned NO and -1 always, so a file could not be read
// through a stream.
-(BOOL)hasBytesAvailable {
    return _status==NSStreamStatusOpen || _status==NSStreamStatusReading;
}

// Bytes read; 0 at the end of the file, then the stream is at its end; -1
// on an error (streamError says which) or when the stream is not open.
-(NSInteger)read:(uint8_t *)buffer maxLength:(NSUInteger)length {
    ssize_t n;

    if(_status==NSStreamStatusAtEnd)
     return 0;
    if(_status!=NSStreamStatusOpen && _status!=NSStreamStatusReading)
     return -1;
    if(length==0)
     return 0;
    _status=NSStreamStatusReading;
    do {
     n=read([_fileHandle fileDescriptor],buffer,length);
    } while(n<0 && errno==EINTR);
    if(n<0){
     [self failWithErrno:errno];
     return -1;
    }
    _status=(n==0)?NSStreamStatusAtEnd:NSStreamStatusOpen;
    return n;
}

@end
