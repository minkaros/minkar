/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGPDFPage.h>
#import <Onyx2D/O2PDFPage.h>
#import "CGConversions.h"

CGPDFPageRef CGPDFPageRetain(CGPDFPageRef self) {
   return [self retain];
}

void CGPDFPageRelease(CGPDFPageRef self) {
   [self release];
}

/* A box as given, its corners in either order; false if it has none. */
static bool boxRect(CGPDFPageRef self,CGPDFBox box,CGRect *rect) {
   CGRect result;

   if(![self getRect:&result forBox:box])
    return false;
   *rect=CGRectStandardize(result);
   return true;
}

CGRect CGPDFPageGetBoxRect(CGPDFPageRef self,CGPDFBox box) {
   CGRect media,result;

   if(self==NULL)
    return CGRectZero;
   if(!boxRect(self,kCGPDFMediaBox,&media))
    media=CGRectMake(0,0,612,792);   // a page without one: US Letter
   if(box==kCGPDFMediaBox)
    return media;
   if(!boxRect(self,kCGPDFCropBox,&result))
    result=media;
   else
    result=CGRectIntersection(result,media);
   if(box==kCGPDFCropBox)
    return result;
   {
    CGRect crop=result;

    if(!boxRect(self,box,&result))
     return crop;
    return CGRectIntersection(result,media);
   }
}

int CGPDFPageGetRotationAngle(CGPDFPageRef self) {
   return (self!=NULL)?[self rotationAngle]:0;
}

size_t CGPDFPageGetPageNumber(CGPDFPageRef self) {
   return (self!=NULL)?[self pageNumber]+1:0;
}

CGPDFDictionaryRef CGPDFPageGetDictionary(CGPDFPageRef self) {
   return (self!=NULL)?(CGPDFDictionaryRef)[self dictionary]:NULL;
}

struct O2PDFDocument *CGPDFPageGetDocument(CGPDFPageRef self) {
   return (self!=NULL)?(struct O2PDFDocument *)[self document]:NULL;
}

CGAffineTransform CGPDFPageGetDrawingTransform(CGPDFPageRef self,CGPDFBox box,CGRect rect,int clockwiseDegrees,bool preserveAspectRatio) {
   return CGAffineTransformFromO2(O2PDFPageGetDrawingTransform(self,box,rect,clockwiseDegrees,preserveAspectRatio));
}

