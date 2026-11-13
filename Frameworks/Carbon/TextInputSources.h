/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Text input sources: the keyboard layouts and input methods the user
 * types with, their properties, and which is selected.
 */

#ifndef __TEXTINPUTSOURCES__
#define __TEXTINPUTSOURCES__

#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN

typedef struct __TISInputSource *TISInputSourceRef;

/* Properties (TISGetInputSourceProperty). */
CF_EXPORT const CFStringRef kTISPropertyInputSourceCategory;
CF_EXPORT const CFStringRef kTISPropertyInputSourceType;
CF_EXPORT const CFStringRef kTISPropertyInputSourceIsASCIICapable;
CF_EXPORT const CFStringRef kTISPropertyInputSourceIsEnableCapable;
CF_EXPORT const CFStringRef kTISPropertyInputSourceIsSelectCapable;
CF_EXPORT const CFStringRef kTISPropertyInputSourceIsEnabled;
CF_EXPORT const CFStringRef kTISPropertyInputSourceIsSelected;
CF_EXPORT const CFStringRef kTISPropertyInputSourceID;
CF_EXPORT const CFStringRef kTISPropertyBundleID;
CF_EXPORT const CFStringRef kTISPropertyInputModeID;
CF_EXPORT const CFStringRef kTISPropertyLocalizedName;
CF_EXPORT const CFStringRef kTISPropertyInputSourceLanguages;
CF_EXPORT const CFStringRef kTISPropertyUnicodeKeyLayoutData;
CF_EXPORT const CFStringRef kTISPropertyIconImageURL;

/* Categories and types (kTISPropertyInputSourceCategory, ...Type). */
CF_EXPORT const CFStringRef kTISCategoryKeyboardInputSource;
CF_EXPORT const CFStringRef kTISCategoryPaletteInputSource;
CF_EXPORT const CFStringRef kTISCategoryInkInputSource;
CF_EXPORT const CFStringRef kTISTypeKeyboardLayout;
CF_EXPORT const CFStringRef kTISTypeKeyboardInputMethodWithoutModes;
CF_EXPORT const CFStringRef kTISTypeKeyboardInputMethodModeEnabled;
CF_EXPORT const CFStringRef kTISTypeKeyboardInputMode;
CF_EXPORT const CFStringRef kTISTypeCharacterPalette;
CF_EXPORT const CFStringRef kTISTypeKeyboardViewer;
CF_EXPORT const CFStringRef kTISTypeInk;

/* Distributed notifications. */
CF_EXPORT const CFStringRef kTISNotifySelectedKeyboardInputSourceChanged;
CF_EXPORT const CFStringRef kTISNotifyEnabledKeyboardInputSourcesChanged;

CF_EXPORT CFTypeID TISInputSourceGetTypeID(void);
CF_EXPORT void *TISGetInputSourceProperty(TISInputSourceRef inputSource, CFStringRef propertyKey);
CF_EXPORT CFArrayRef TISCreateInputSourceList(CFDictionaryRef properties, Boolean includeAllInstalled);
CF_EXPORT CFArrayRef TISCreateASCIICapableInputSourceList(void);
CF_EXPORT TISInputSourceRef TISCopyCurrentKeyboardInputSource(void);
CF_EXPORT TISInputSourceRef TISCopyCurrentKeyboardLayoutInputSource(void);
CF_EXPORT TISInputSourceRef TISCopyCurrentASCIICapableKeyboardInputSource(void);
CF_EXPORT TISInputSourceRef TISCopyCurrentASCIICapableKeyboardLayoutInputSource(void);
CF_EXPORT TISInputSourceRef TISCopyInputSourceForLanguage(CFStringRef language);
CF_EXPORT TISInputSourceRef TISCopyInputMethodKeyboardLayoutOverride(void);
CF_EXPORT OSStatus TISSelectInputSource(TISInputSourceRef inputSource);
CF_EXPORT OSStatus TISDeselectInputSource(TISInputSourceRef inputSource);
CF_EXPORT OSStatus TISEnableInputSource(TISInputSourceRef inputSource);
CF_EXPORT OSStatus TISDisableInputSource(TISInputSourceRef inputSource);

CF_EXTERN_C_END

#endif
