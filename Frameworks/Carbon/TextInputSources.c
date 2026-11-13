/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A constant's value is its name without "k".
 */

#include "TextInputSources.h"

#define KEY(name) const CFStringRef k##name = CFSTR(#name)

KEY(TISPropertyInputSourceCategory);
KEY(TISPropertyInputSourceType);
KEY(TISPropertyInputSourceIsASCIICapable);
KEY(TISPropertyInputSourceIsEnableCapable);
KEY(TISPropertyInputSourceIsSelectCapable);
KEY(TISPropertyInputSourceIsEnabled);
KEY(TISPropertyInputSourceIsSelected);
KEY(TISPropertyInputSourceID);
KEY(TISPropertyBundleID);
KEY(TISPropertyInputModeID);
KEY(TISPropertyLocalizedName);
KEY(TISPropertyInputSourceLanguages);
KEY(TISPropertyUnicodeKeyLayoutData);
KEY(TISPropertyIconImageURL);
KEY(TISCategoryKeyboardInputSource);
KEY(TISCategoryPaletteInputSource);
KEY(TISCategoryInkInputSource);
KEY(TISTypeKeyboardLayout);
KEY(TISTypeKeyboardInputMethodWithoutModes);
KEY(TISTypeKeyboardInputMethodModeEnabled);
KEY(TISTypeKeyboardInputMode);
KEY(TISTypeCharacterPalette);
KEY(TISTypeKeyboardViewer);
KEY(TISTypeInk);
KEY(TISNotifySelectedKeyboardInputSourceChanged);
KEY(TISNotifyEnabledKeyboardInputSourcesChanged);
