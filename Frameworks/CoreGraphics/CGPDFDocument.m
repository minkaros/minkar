/* Copyright(c) 2008 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CoreGraphics.h>
#import <Onyx2D/O2PDFDocument.h>

CGPDFDocumentRef CGPDFDocumentRetain(CGPDFDocumentRef self) {
   return [self retain];
}

void CGPDFDocumentRelease(CGPDFDocumentRef self) {
   [self release];
}

CGPDFDocumentRef CGPDFDocumentCreateWithProvider(CGDataProviderRef provider) {
   return [[O2PDFDocument alloc] initWithDataProvider:provider];
}

CGPDFDocumentRef CGPDFDocumentCreateWithURL(CFURLRef url) {
   CGDataProviderRef provider=(url!=NULL)?CGDataProviderCreateWithURL(url):NULL;
   CGPDFDocumentRef  document;

   if(provider==NULL)
    return NULL;
   document=CGPDFDocumentCreateWithProvider(provider);
   CGDataProviderRelease(provider);
   return document;
}

size_t CGPDFDocumentGetNumberOfPages(CGPDFDocumentRef self) {
   return [self pageCount];
}

CGPDFPageRef CGPDFDocumentGetPage(CGPDFDocumentRef self,size_t pageNumber) {
   if(self==NULL || pageNumber<1 || pageNumber>(size_t)[self pageCount])
    return NULL;
   return [self pageAtNumber:pageNumber];
}

/* ---- Minkar: the document's structure ---- */

#import <Onyx2D/O2PDFDictionary.h>
#import <Onyx2D/O2PDFArray.h>
#import <Onyx2D/O2PDFObject_Integer.h>
#import <Onyx2D/O2PDFxref.h>

CGPDFDictionaryRef CGPDFDocumentGetCatalog(CGPDFDocumentRef self) {
   return (self!=NULL)?(CGPDFDictionaryRef)[self catalog]:NULL;
}

CGPDFDictionaryRef CGPDFDocumentGetInfo(CGPDFDocumentRef self) {
   return (self!=NULL)?(CGPDFDictionaryRef)[self infoDictionary]:NULL;
}

CGPDFArrayRef CGPDFDocumentGetID(CGPDFDocumentRef self) {
   O2PDFArray *result;

   if(self==NULL || ![[[self xref] trailer] getArrayForKey:"ID" value:&result])
    return NULL;
   return (CGPDFArrayRef)result;
}

void CGPDFDocumentGetVersion(CGPDFDocumentRef self,int *majorVersion,int *minorVersion) {
   if(self!=NULL)
    [self getVersionMajor:majorVersion minor:minorVersion];
   else {
    if(majorVersion!=NULL) *majorVersion=0;
    if(minorVersion!=NULL) *minorVersion=0;
   }
}

bool CGPDFDocumentIsEncrypted(CGPDFDocumentRef self) {
   return (self!=NULL && [self encryptDictionary]!=nil);
}

bool CGPDFDocumentIsUnlocked(CGPDFDocumentRef self) {
   return (self!=NULL && !CGPDFDocumentIsEncrypted(self));
}

bool CGPDFDocumentUnlockWithPassword(CGPDFDocumentRef self,const char *password) {
   return CGPDFDocumentIsUnlocked(self);
}

/* An encrypted document's permissions (its P entry: bit 3 printing, bit 5
   copying); an open one allows both. */
static bool allows(CGPDFDocumentRef self,int bit) {
   O2PDFInteger permissions;

   if(self==NULL)
    return false;
   if(!CGPDFDocumentIsEncrypted(self))
    return true;
   if(![[self encryptDictionary] getIntegerForKey:"P" value:&permissions])
    return false;
   return (permissions&(1<<(bit-1)))!=0;
}

bool CGPDFDocumentAllowsPrinting(CGPDFDocumentRef self) {
   return allows(self,3);
}

bool CGPDFDocumentAllowsCopying(CGPDFDocumentRef self) {
   return allows(self,5);
}
