/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CF type IDs for the CoreGraphics types, which are Onyx2D objects:
 * CFGetTypeID asks the object (-_cfTypeID) and gets its type's ID,
 * registered on first use.
 */

#import <CoreGraphics/CoreGraphics.h>
#import <CoreGraphics/CGPDFDocument.h>
#import <CoreGraphics/CGPDFPage.h>
#import <Foundation/Foundation.h>
#import <Onyx2D/O2Image.h>
#import <Onyx2D/O2Color.h>
#import <Onyx2D/O2ColorSpace.h>
#import <Onyx2D/O2Path.h>
#import <Onyx2D/O2Font.h>
#import <Onyx2D/O2DataProvider.h>
#import <Onyx2D/O2DataConsumer.h>
#import <Onyx2D/O2Shading.h>
#import <Onyx2D/O2Function.h>
#import <Onyx2D/O2Pattern.h>
#import <Onyx2D/O2Layer.h>
#import <Onyx2D/O2Context.h>
#import <Onyx2D/O2PDFDocument.h>
#import <Onyx2D/O2PDFPage.h>

extern CFTypeID _CFRuntimeRegisterClass(const void *runtimeClass);

#define CG_TYPE(name, objcClass) \
   static const struct { CFIndex version; const char *cls; void *functions[16]; } name##RuntimeClass={ 0, #name, { NULL } }; \
   CFTypeID name##GetTypeID(void) { \
      static CFTypeID typeID; \
      if(typeID==0) \
       typeID=_CFRuntimeRegisterClass(&name##RuntimeClass); \
      return typeID; \
   } \
   @interface objcClass (CFType) @end \
   @implementation objcClass (CFType) \
   -(CFTypeID)_cfTypeID { return name##GetTypeID(); } \
   @end

CG_TYPE(CGImage, O2Image)
CG_TYPE(CGColor, O2Color)
CG_TYPE(CGColorSpace, O2ColorSpace)
CG_TYPE(CGPath, O2Path)
CG_TYPE(CGFont, O2Font)
CG_TYPE(CGDataProvider, O2DataProvider)
CG_TYPE(CGDataConsumer, O2DataConsumer)
CG_TYPE(CGShading, O2Shading)
CG_TYPE(CGFunction, O2Function)
CG_TYPE(CGPattern, O2Pattern)
CG_TYPE(CGLayer, O2Layer)
CG_TYPE(CGContext, O2Context)
CG_TYPE(CGPDFDocument, O2PDFDocument)
CG_TYPE(CGPDFPage, O2PDFPage)
