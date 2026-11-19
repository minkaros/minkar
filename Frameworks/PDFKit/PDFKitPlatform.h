/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#ifndef PDFKIT_EXTERN
#ifdef __cplusplus
#define PDFKIT_EXTERN extern "C" __attribute__((visibility("default")))
#else
#define PDFKIT_EXTERN extern __attribute__((visibility("default")))
#endif
#endif
#define PDFKIT_CLASS_AVAILABLE(mac, ios) __attribute__((visibility("default")))
#define PDFKIT_AVAILABLE(mac, ios)
#define PDFKIT_DEPRECATED(macIntro, macDep, iosIntro, iosDep)

typedef NSView PDFKitPlatformView;
typedef NSColor PDFKitPlatformColor;
typedef NSBezierPath PDFKitPlatformBezierPath;
typedef NSImage PDFKitPlatformImage;
typedef NSFont PDFKitPlatformFont;
typedef NSEdgeInsets PDFEdgeInsets;
typedef CGPoint PDFPoint;
typedef CGSize PDFSize;
typedef CGRect PDFRect;
