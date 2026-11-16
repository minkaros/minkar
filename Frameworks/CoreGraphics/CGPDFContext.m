/* Copyright (c) 2009 Christopher J. W. Lloyd <cjwl@objc.net>
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <CoreGraphics/CGPDFContext.h>
#import <Onyx2D/O2PDFContext.h>

const CFStringRef kCGPDFContextTitle=(CFStringRef)@"kCGPDFContextTitle";
const CFStringRef kCGPDFContextAuthor=(CFStringRef)@"kCGPDFContextAuthor";
const CFStringRef kCGPDFContextSubject=(CFStringRef)@"kCGPDFContextSubject";
const CFStringRef kCGPDFContextKeywords=(CFStringRef)@"kCGPDFContextKeywords";
const CFStringRef kCGPDFContextCreator=(CFStringRef)@"kCGPDFContextCreator";
const CFStringRef kCGPDFContextMediaBox=(CFStringRef)@"kCGPDFContextMediaBox";
const CFStringRef kCGPDFContextCropBox=(CFStringRef)@"kCGPDFContextCropBox";
const CFStringRef kCGPDFContextBleedBox=(CFStringRef)@"kCGPDFContextBleedBox";
const CFStringRef kCGPDFContextTrimBox=(CFStringRef)@"kCGPDFContextTrimBox";
const CFStringRef kCGPDFContextArtBox=(CFStringRef)@"kCGPDFContextArtBox";

CGContextRef CGPDFContextCreate(CGDataConsumerRef consumer,const CGRect *mediaBox,CFDictionaryRef auxiliaryInfo) {
   return [[O2PDFContext alloc] initWithConsumer:consumer mediaBox:mediaBox auxiliaryInfo:(NSDictionary *)auxiliaryInfo];
}

/* A PDF written to a file URL. */
CGContextRef CGPDFContextCreateWithURL(CFURLRef url,const CGRect *mediaBox,CFDictionaryRef auxiliaryInfo) {
   CGDataConsumerRef consumer=url!=NULL?CGDataConsumerCreateWithURL(url):NULL;

   if(consumer==NULL)
    return NULL;
   CGContextRef context=CGPDFContextCreate(consumer,mediaBox,auxiliaryInfo);
   CGDataConsumerRelease(consumer);
   return context;
}

void CGPDFContextBeginPage(CGContextRef context,CFDictionaryRef pageInfo) {
   if([(id)context isKindOfClass:[O2PDFContext class]])
    [(O2PDFContext *)context beginPageWithInfo:(NSDictionary *)pageInfo];
}

void CGPDFContextEndPage(CGContextRef context) {
   CGContextEndPage(context);
}

void CGPDFContextAddDocumentMetadata(CGContextRef context,CFDataRef metadata) {
   if([(id)context isKindOfClass:[O2PDFContext class]] && metadata!=NULL)
    [(O2PDFContext *)context addDocumentMetadata:(NSData *)metadata];
}

COREGRAPHICS_EXPORT void CGPDFContextClose(CGContextRef self) {
   [self close];
}

/* Minkar: links and named destinations of the page being drawn. */
void CGPDFContextSetURLForRect(CGContextRef context,CFURLRef url,CGRect rect) {
   if([(id)context isKindOfClass:[O2PDFContext class]] && url!=NULL)
    [(O2PDFContext *)context setURL:(NSURL *)url forRect:rect];
}

void CGPDFContextSetDestinationForRect(CGContextRef context,CFStringRef name,CGRect rect) {
   if([(id)context isKindOfClass:[O2PDFContext class]] && name!=NULL)
    [(O2PDFContext *)context setDestination:(NSString *)name forRect:rect];
}

void CGPDFContextAddDestinationAtPoint(CGContextRef context,CFStringRef name,CGPoint point) {
   if([(id)context isKindOfClass:[O2PDFContext class]] && name!=NULL)
    [(O2PDFContext *)context addDestination:(NSString *)name atPoint:point];
}
