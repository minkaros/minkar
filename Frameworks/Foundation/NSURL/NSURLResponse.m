/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSURLResponse.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSRaise.h>

@implementation NSURLResponse

-initWithURL:(NSURL *)url MIMEType:(NSString *)mimeType expectedContentLength:(NSInteger)expectedLength textEncodingName:(NSString *)encoding {
   _url=[url retain];
   _mimeType=[mimeType retain];
   _expectedContentLength=expectedLength;
   _encoding=[encoding retain];
   return self;
}

-(void)dealloc {
   [_url release];
   [_mimeType release];
   [_encoding release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-initWithCoder:(NSCoder *)coder {
   NSUnimplementedMethod();
   return nil;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   NSUnimplementedMethod();
}


- (NSURL *)URL
{
   return _url;
}


-(NSString *)MIMEType {
   return _mimeType;
}

-(long long)expectedContentLength {
   return _expectedContentLength;
}

-(NSString *)textEncodingName {
   return _encoding;
}

/* Minkar: the file name a response would be saved under, as the
   reference gives it: the server's (Content-Disposition, filename* before
   filename), else the URL's last path component, else "Unknown"; with the
   type's extension when it has none (not for unknown binary data); never
   a path. */
static NSString *dispositionFilename(NSString *disposition) {
   NSString *plain=nil,*extended=nil;

   for(NSString *part in [disposition componentsSeparatedByString:@";"]){
    NSString *item=[part stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    NSRange equals=[item rangeOfString:@"="];

    if(equals.location==NSNotFound)
     continue;
    NSString *key=[[item substringToIndex:equals.location] lowercaseString];
    NSString *value=[[item substringFromIndex:NSMaxRange(equals)] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

    if([key isEqualToString:@"filename*"]){
     // charset'language'percent-encoded
     NSArray *pieces=[value componentsSeparatedByString:@"'"];
     if([pieces count]==3)
      extended=[[pieces objectAtIndex:2] stringByRemovingPercentEncoding];
    }
    else if([key isEqualToString:@"filename"]){
     if([value length]>=2 && [value hasPrefix:@"\""] && [value hasSuffix:@"\""])
      value=[value substringWithRange:NSMakeRange(1,[value length]-2)];
     plain=value;
    }
   }
   return extended!=nil?extended:plain;
}

static NSString *extensionForMIMEType(NSString *type) {
   static NSDictionary *extensions;

   if(extensions==nil)
    extensions=[[NSDictionary alloc] initWithObjectsAndKeys:
     @"html",@"text/html", @"txt",@"text/plain", @"css",@"text/css", @"csv",@"text/csv",
     @"xml",@"text/xml", @"xml",@"application/xml", @"xhtml",@"application/xhtml+xml",
     @"js",@"text/javascript", @"js",@"application/javascript", @"json",@"application/json",
     @"pdf",@"application/pdf", @"zip",@"application/zip", @"gz",@"application/gzip",
     @"rtf",@"text/rtf", @"rtf",@"application/rtf",
     @"png",@"image/png", @"jpeg",@"image/jpeg", @"gif",@"image/gif", @"svg",@"image/svg+xml",
     @"webp",@"image/webp", @"tiff",@"image/tiff", @"ico",@"image/x-icon",
     @"mp3",@"audio/mpeg", @"m4a",@"audio/mp4", @"wav",@"audio/wav", @"ogg",@"audio/ogg",
     @"mp4",@"video/mp4", @"mov",@"video/quicktime", @"webm",@"video/webm", nil];
   return [extensions objectForKey:[type lowercaseString]];
}

-(NSString *)suggestedFilename {
   NSString *name=nil;

   if([self respondsToSelector:@selector(allHeaderFields)]){
    NSDictionary *headers=[(id)self allHeaderFields];

    for(NSString *key in headers)
     if([key caseInsensitiveCompare:@"Content-Disposition"]==NSOrderedSame)
      name=dispositionFilename([headers objectForKey:key]);
   }
   name=[name lastPathComponent];
   if([name length]==0 || [name isEqualToString:@"/"]){
    name=[[[_url path] lastPathComponent] stringByRemovingPercentEncoding];
    if([name length]==0 || [name isEqualToString:@"/"])
     name=@"Unknown";
   }
   if([[name pathExtension] length]==0){
    NSString *extension=extensionForMIMEType(_mimeType);

    if(extension!=nil)
     name=[name stringByAppendingPathExtension:extension];
   }
   return name;
}


@end
