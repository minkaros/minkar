/* Copyright (c) 2007 Johannes Fortmann

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSObject.h>
#import <AppKit/AppKitExport.h>

@class NSDictionary, NSArray, NSString;

@interface NSObject (BindingSupport)
- (void)bind:(id)binding toObject:(id)destination withKeyPath:(NSString *)keyPath options:(NSDictionary *)options;
- (NSDictionary *)infoForBinding:(id)binding;
- (void)unbind:(id)binding;
+ (void)exposeBinding:(id)binding;
@end

@interface NSObject (InternalBindingSupport)
// returns a dictionary with the default options for the specified binding
// the dictionaries are stored on a per-class basis in defaultBindingOptions.plist
// the binding parameter is currently ignored
- (NSDictionary *)_defaultBindingOptionsForBinding:(NSString *)binding;

// return the class of a suitable binder for the respective binding
+ (Class)_binderClassForBinding:(id)binding;

// return the respective binder or nil if unbound
- (id)_binderForBinding:(id)binding;

// return a suitable binder for the binding. if create is NO, only returns a binder
// if the binding is actually in use
- (id)_binderForBinding:(id)binding create:(BOOL)create;

// return a key path suitable for KVO to replace the generic binding name
// e.g. "value" -> "color" on NSColorWell
- (id)_replacementKeyPathForBinding:(id)binding;

// returns all binders used by the object
- (NSArray *)_allUsedBinders;

// unbinds all bindings used; should be called in -dealloc
- (void)_unbindAllBindings;

// the currently set value is a placeholder and should be displayed accordingly
- (id)_setCurrentValueIsPlaceholder:(BOOL)isPlaceholder;
@end

@interface NSObject (NSEditor)
- (BOOL)commitEditing;
- (void)discardEditing;
@end

@interface NSObject (NSEditorRegistration)
- (void)objectDidBeginEditing:editor;
- (void)objectDidEndEditing:editor;
@end

APPKIT_EXPORT NSString *const NSObservedObjectKey;
APPKIT_EXPORT NSString *const NSObservedKeyPathKey;
APPKIT_EXPORT NSString *const NSOptionsKey;

// Binding option keys
APPKIT_EXPORT NSString *const NSNullPlaceholderBindingOption;
APPKIT_EXPORT NSString *const NSNoSelectionPlaceholderBindingOption;
APPKIT_EXPORT NSString *const NSMultipleValuesPlaceholderBindingOption;
APPKIT_EXPORT NSString *const NSCreatesSortDescriptorBindingOption;
APPKIT_EXPORT NSString *const NSRaisesForNotApplicableKeysBindingOption;
APPKIT_EXPORT NSString *const NSAllowsEditingMultipleValuesSelectionBindingOption;
APPKIT_EXPORT NSString *const NSValueTransformerNameBindingOption;
APPKIT_EXPORT NSString *const NSValueTransformerBindingOption;
APPKIT_EXPORT NSString *const NSConditionallySetsEnabledBindingOption;
APPKIT_EXPORT NSString *const NSConditionallySetsEditableBindingOption;
APPKIT_EXPORT NSString *const NSContinuouslyUpdatesValueBindingOption;
APPKIT_EXPORT NSString *const NSDisplayPatternBindingOption;

/* The bindings' names (bind:toObject:withKeyPath:options:). */
APPKIT_EXPORT NSString *const NSAlignmentBinding;
APPKIT_EXPORT NSString *const NSAlternateImageBinding;
APPKIT_EXPORT NSString *const NSAlternateTitleBinding;
APPKIT_EXPORT NSString *const NSAnimateBinding;
APPKIT_EXPORT NSString *const NSAnimationDelayBinding;
APPKIT_EXPORT NSString *const NSArgumentBinding;
APPKIT_EXPORT NSString *const NSAttributedStringBinding;
APPKIT_EXPORT NSString *const NSContentArrayBinding;
APPKIT_EXPORT NSString *const NSContentArrayForMultipleSelectionBinding;
APPKIT_EXPORT NSString *const NSContentBinding;
APPKIT_EXPORT NSString *const NSContentDictionaryBinding;
APPKIT_EXPORT NSString *const NSContentHeightBinding;
APPKIT_EXPORT NSString *const NSContentObjectBinding;
APPKIT_EXPORT NSString *const NSContentObjectsBinding;
APPKIT_EXPORT NSString *const NSContentSetBinding;
APPKIT_EXPORT NSString *const NSContentValuesBinding;
APPKIT_EXPORT NSString *const NSContentWidthBinding;
APPKIT_EXPORT NSString *const NSCriticalValueBinding;
APPKIT_EXPORT NSString *const NSDataBinding;
APPKIT_EXPORT NSString *const NSDisplayPatternTitleBinding;
APPKIT_EXPORT NSString *const NSDisplayPatternValueBinding;
APPKIT_EXPORT NSString *const NSDocumentEditedBinding;
APPKIT_EXPORT NSString *const NSDoubleClickArgumentBinding;
APPKIT_EXPORT NSString *const NSDoubleClickTargetBinding;
APPKIT_EXPORT NSString *const NSEditableBinding;
APPKIT_EXPORT NSString *const NSEnabledBinding;
APPKIT_EXPORT NSString *const NSExcludedKeysBinding;
APPKIT_EXPORT NSString *const NSFilterPredicateBinding;
APPKIT_EXPORT NSString *const NSFontBinding;
APPKIT_EXPORT NSString *const NSFontBoldBinding;
APPKIT_EXPORT NSString *const NSFontFamilyNameBinding;
APPKIT_EXPORT NSString *const NSFontItalicBinding;
APPKIT_EXPORT NSString *const NSFontNameBinding;
APPKIT_EXPORT NSString *const NSFontSizeBinding;
APPKIT_EXPORT NSString *const NSHeaderTitleBinding;
APPKIT_EXPORT NSString *const NSHiddenBinding;
APPKIT_EXPORT NSString *const NSImageBinding;
APPKIT_EXPORT NSString *const NSIncludedKeysBinding;
APPKIT_EXPORT NSString *const NSInitialKeyBinding;
APPKIT_EXPORT NSString *const NSInitialValueBinding;
APPKIT_EXPORT NSString *const NSIsIndeterminateBinding;
APPKIT_EXPORT NSString *const NSLabelBinding;
APPKIT_EXPORT NSString *const NSLocalizableKeysBinding;
APPKIT_EXPORT NSString *const NSManagedObjectContextBinding;
APPKIT_EXPORT NSString *const NSMaximumRecentsBinding;
APPKIT_EXPORT NSString *const NSMaxValueBinding;
APPKIT_EXPORT NSString *const NSMaxWidthBinding;
APPKIT_EXPORT NSString *const NSMinValueBinding;
APPKIT_EXPORT NSString *const NSMinWidthBinding;
APPKIT_EXPORT NSString *const NSMixedStateImageBinding;
APPKIT_EXPORT NSString *const NSOffStateImageBinding;
APPKIT_EXPORT NSString *const NSOnStateImageBinding;
APPKIT_EXPORT NSString *const NSPositioningRectBinding;
APPKIT_EXPORT NSString *const NSPredicateBinding;
APPKIT_EXPORT NSString *const NSRecentSearchesBinding;
APPKIT_EXPORT NSString *const NSRepresentedFilenameBinding;
APPKIT_EXPORT NSString *const NSRowHeightBinding;
APPKIT_EXPORT NSString *const NSSelectedIdentifierBinding;
APPKIT_EXPORT NSString *const NSSelectedIndexBinding;
APPKIT_EXPORT NSString *const NSSelectedLabelBinding;
APPKIT_EXPORT NSString *const NSSelectedObjectBinding;
APPKIT_EXPORT NSString *const NSSelectedObjectsBinding;
APPKIT_EXPORT NSString *const NSSelectedTagBinding;
APPKIT_EXPORT NSString *const NSSelectedValueBinding;
APPKIT_EXPORT NSString *const NSSelectedValuesBinding;
APPKIT_EXPORT NSString *const NSSelectionIndexesBinding;
APPKIT_EXPORT NSString *const NSSelectionIndexPathsBinding;
APPKIT_EXPORT NSString *const NSSortDescriptorsBinding;
APPKIT_EXPORT NSString *const NSTargetBinding;
APPKIT_EXPORT NSString *const NSTextColorBinding;
APPKIT_EXPORT NSString *const NSTitleBinding;
APPKIT_EXPORT NSString *const NSToolTipBinding;
APPKIT_EXPORT NSString *const NSTransparentBinding;
APPKIT_EXPORT NSString *const NSValueBinding;
APPKIT_EXPORT NSString *const NSValuePathBinding;
APPKIT_EXPORT NSString *const NSValueURLBinding;
APPKIT_EXPORT NSString *const NSVisibleBinding;
APPKIT_EXPORT NSString *const NSWarningValueBinding;
APPKIT_EXPORT NSString *const NSWidthBinding;

/* More option keys. */
APPKIT_EXPORT NSString *const NSAlwaysPresentsApplicationModalAlertsBindingOption;
APPKIT_EXPORT NSString *const NSContentPlacementTagBindingOption;
APPKIT_EXPORT NSString *const NSDeletesObjectsOnRemoveBindingsOption;
APPKIT_EXPORT NSString *const NSHandlesContentAsCompoundValueBindingOption;
APPKIT_EXPORT NSString *const NSInsertsNullPlaceholderBindingOption;
APPKIT_EXPORT NSString *const NSInvokesSeparatelyWithArrayObjectsBindingOption;
APPKIT_EXPORT NSString *const NSNotApplicablePlaceholderBindingOption;
APPKIT_EXPORT NSString *const NSPredicateFormatBindingOption;
APPKIT_EXPORT NSString *const NSSelectorNameBindingOption;
APPKIT_EXPORT NSString *const NSSelectsAllWhenSettingContentBindingOption;
APPKIT_EXPORT NSString *const NSValidatesImmediatelyBindingOption;

enum {
    kNSBindingDebugLogLevel1 = 1,
    kNSBindingDebugLogLevel2,
    kNSBindingDebugLogLevel3
};

APPKIT_EXPORT void NSDetermineBindingDebugLoggingLevel();

APPKIT_EXPORT int NSBindingDebugLogLevel; // Defaults to 0 = no logging

#define NSBindingDebugLog(level, format, args...) \
    NSDetermineBindingDebugLoggingLevel();        \
    if(NSBindingDebugLogLevel >= level)           \
    NSLog(@"%d: %s line: %d | %@", level, __PRETTY_FUNCTION__, __LINE__, [NSString stringWithFormat:format, ##args])
