/* Copyright (c) 2007 Johannes Fortmann

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import "NSKVOBinder.h"
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSString.h>
#import <AppKit/NSObject+BindingSupport.h>
#import <AppKit/NSController.h>
#import <AppKit/NSObservationProxy.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSKeyValueCoding.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSUserDefaults.h>

static NSMutableDictionary *bindersForObjects=nil;
static NSDictionary *defaultBindingOptions=nil;
static NSMutableDictionary *defaultBindingOptionsCache=nil;

NSString * const NSObservedObjectKey=@"NSObservedObject";
NSString * const NSObservedKeyPathKey=@"NSObservedKeyPath";
NSString * const NSOptionsKey=@"NSOptions";

NSString * const NSNullPlaceholderBindingOption=@"NSNullPlaceholder"; // Do not change.

NSString * const NSAlignmentBinding=@"alignment";
NSString * const NSAlternateImageBinding=@"alternateImage";
NSString * const NSAlternateTitleBinding=@"alternateTitle";
NSString * const NSAnimateBinding=@"animate";
NSString * const NSAnimationDelayBinding=@"animationDelay";
NSString * const NSArgumentBinding=@"argument";
NSString * const NSAttributedStringBinding=@"attributedString";
NSString * const NSContentArrayBinding=@"contentArray";
NSString * const NSContentArrayForMultipleSelectionBinding=@"contentArrayForMultipleSelection";
NSString * const NSContentBinding=@"content";
NSString * const NSContentDictionaryBinding=@"contentDictionary";
NSString * const NSContentHeightBinding=@"contentHeight";
NSString * const NSContentObjectBinding=@"contentObject";
NSString * const NSContentObjectsBinding=@"contentObjects";
NSString * const NSContentSetBinding=@"contentSet";
NSString * const NSContentValuesBinding=@"contentValues";
NSString * const NSContentWidthBinding=@"contentWidth";
NSString * const NSCriticalValueBinding=@"criticalValue";
NSString * const NSDataBinding=@"data";
NSString * const NSDisplayPatternTitleBinding=@"displayPatternTitle";
NSString * const NSDisplayPatternValueBinding=@"displayPatternValue";
NSString * const NSDocumentEditedBinding=@"documentEdited";
NSString * const NSDoubleClickArgumentBinding=@"doubleClickArgument";
NSString * const NSDoubleClickTargetBinding=@"doubleClickTarget";
NSString * const NSEditableBinding=@"editable";
NSString * const NSEnabledBinding=@"enabled";
NSString * const NSExcludedKeysBinding=@"excludedKeys";
NSString * const NSFilterPredicateBinding=@"filterPredicate";
NSString * const NSFontBinding=@"font";
NSString * const NSFontBoldBinding=@"fontBold";
NSString * const NSFontFamilyNameBinding=@"fontFamilyName";
NSString * const NSFontItalicBinding=@"fontItalic";
NSString * const NSFontNameBinding=@"fontName";
NSString * const NSFontSizeBinding=@"fontSize";
NSString * const NSHeaderTitleBinding=@"headerTitle";
NSString * const NSHiddenBinding=@"hidden";
NSString * const NSImageBinding=@"image";
NSString * const NSIncludedKeysBinding=@"includedKeys";
NSString * const NSInitialKeyBinding=@"initialKey";
NSString * const NSInitialValueBinding=@"initialValue";
NSString * const NSIsIndeterminateBinding=@"isIndeterminate";
NSString * const NSLabelBinding=@"label";
NSString * const NSLocalizableKeysBinding=@"localizableKeys";
NSString * const NSManagedObjectContextBinding=@"managedObjectContext";
NSString * const NSMaximumRecentsBinding=@"maximumRecents";
NSString * const NSMaxValueBinding=@"maxValue";
NSString * const NSMaxWidthBinding=@"maxWidth";
NSString * const NSMinValueBinding=@"minValue";
NSString * const NSMinWidthBinding=@"minWidth";
NSString * const NSMixedStateImageBinding=@"mixedStateImage";
NSString * const NSOffStateImageBinding=@"offStateImage";
NSString * const NSOnStateImageBinding=@"onStateImage";
NSString * const NSPositioningRectBinding=@"positioningRect";
NSString * const NSPredicateBinding=@"predicate";
NSString * const NSRecentSearchesBinding=@"recentSearches";
NSString * const NSRepresentedFilenameBinding=@"representedFilename";
NSString * const NSRowHeightBinding=@"rowHeight";
NSString * const NSSelectedIdentifierBinding=@"selectedIdentifier";
NSString * const NSSelectedIndexBinding=@"selectedIndex";
NSString * const NSSelectedLabelBinding=@"selectedLabel";
NSString * const NSSelectedObjectBinding=@"selectedObject";
NSString * const NSSelectedObjectsBinding=@"selectedObjects";
NSString * const NSSelectedTagBinding=@"selectedTag";
NSString * const NSSelectedValueBinding=@"selectedValue";
NSString * const NSSelectedValuesBinding=@"selectedValues";
NSString * const NSSelectionIndexesBinding=@"selectionIndexes";
NSString * const NSSelectionIndexPathsBinding=@"selectionIndexPaths";
NSString * const NSSortDescriptorsBinding=@"sortDescriptors";
NSString * const NSTargetBinding=@"target";
NSString * const NSTextColorBinding=@"textColor";
NSString * const NSTitleBinding=@"title";
NSString * const NSToolTipBinding=@"toolTip";
NSString * const NSTransparentBinding=@"transparent";
NSString * const NSValueBinding=@"value";
NSString * const NSValuePathBinding=@"valuePath";
NSString * const NSValueURLBinding=@"valueURL";
NSString * const NSVisibleBinding=@"visible";
NSString * const NSWarningValueBinding=@"warningValue";
NSString * const NSWidthBinding=@"width";
NSString * const NSAlwaysPresentsApplicationModalAlertsBindingOption=@"NSAlwaysPresentsApplicationModalAlerts";
NSString * const NSContentPlacementTagBindingOption=@"NSContentPlacementTag";
NSString * const NSDeletesObjectsOnRemoveBindingsOption=@"NSDeletesObjectsOnRemove";
NSString * const NSHandlesContentAsCompoundValueBindingOption=@"NSHandlesContentAsCompoundValue";
NSString * const NSInsertsNullPlaceholderBindingOption=@"NSInsertsNullPlaceholder";
NSString * const NSInvokesSeparatelyWithArrayObjectsBindingOption=@"NSInvokesSeparatelyWithArrayObjects";
NSString * const NSNotApplicablePlaceholderBindingOption=@"NSNotApplicablePlaceholder";
NSString * const NSPredicateFormatBindingOption=@"NSPredicateFormat";
NSString * const NSSelectorNameBindingOption=@"NSSelectorName";
NSString * const NSSelectsAllWhenSettingContentBindingOption=@"NSSelectsAllWhenSettingContent";
NSString * const NSValidatesImmediatelyBindingOption=@"NSValidatesImmediately";
NSString * const NSNoSelectionPlaceholderBindingOption=@"NSNoSelectionPlaceholder";
NSString * const NSMultipleValuesPlaceholderBindingOption=@"NSMultipleValuesPlaceholder";
NSString * const NSCreatesSortDescriptorBindingOption=@"NSCreatesSortDescriptors";
NSString * const NSRaisesForNotApplicableKeysBindingOption=@"NSRaisesForNotApplicableKeys";
NSString * const NSAllowsEditingMultipleValuesSelectionBindingOption=@"NSAllowsEditingMultipleValuesSelection";
NSString * const NSValueTransformerNameBindingOption=@"NSValueTransformerName"; // Do not change.
NSString * const NSValueTransformerBindingOption=@"NSValueTransformerBinding";
NSString * const NSConditionallySetsEnabledBindingOption=@"NSConditionallySetsEnabled"; // Do not change.
NSString * const NSConditionallySetsEditableBindingOption=@"NSConditionallySetsEditable"; // Do not change.
NSString * const NSContinuouslyUpdatesValueBindingOption=@"NSContinuouslyUpdatesValue"; // Do not change.
NSString * const NSDisplayPatternBindingOption=@"NSDisplayPattern"; // Do not change.



int NSBindingDebugLogLevel = 0; // Defaults to no logging

@implementation NSObject (BindingSupport)

void NSDetermineBindingDebugLoggingLevel(void)
{
        static BOOL loggingLevelDetermined = NO;
        if (loggingLevelDetermined == NO) {
                NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
                NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

                NSBindingDebugLogLevel = [defaults integerForKey: @"NSBindingDebugLogLevel"];
                if (NSBindingDebugLogLevel > 0) {
                        NSLog(@"set NSBindingDebugLogLevel to: '%d'", NSBindingDebugLogLevel);
                }
                [pool drain];
                loggingLevelDetermined = YES;
        }
}


+(NSDictionary *)_defaultBindingOptionsForBinding:(NSString *)binding {

   if(defaultBindingOptions==nil) {
    NSBundle *bundle=[NSBundle bundleForClass:[_NSKVOBinder class]];
    NSString *path=[bundle pathForResource:@"defaultBindingOptions" ofType:@"plist"];

    if((defaultBindingOptions=[[NSDictionary alloc] initWithContentsOfFile:path])==nil)
     defaultBindingOptions=[NSDictionary new];
   }

   NSString     *className=NSStringFromClass(self);
   NSString     *bindingKey=[[className stringByAppendingString:@"."] stringByAppendingString:binding];
   NSDictionary *defaults=[defaultBindingOptionsCache objectForKey:bindingKey];

   if(defaults==nil){
    NSMutableDictionary *values;

        if(self==[NSObject class])
     values=[NSMutableDictionary dictionary];
    else
     values=[[[[self superclass] _defaultBindingOptionsForBinding:binding] mutableCopy] autorelease];

    [values addEntriesFromDictionary:[defaultBindingOptions objectForKey:bindingKey]];

    if(defaultBindingOptionsCache==nil)
     defaultBindingOptionsCache=[NSMutableDictionary new];

    [defaultBindingOptionsCache setObject:values forKey:bindingKey];
    defaults=values;
   }

   return defaults;
}

-(NSDictionary *)_defaultBindingOptionsForBinding:(NSString *)binding {
        return [[self class] _defaultBindingOptionsForBinding:binding];
}


+(Class)_binderClassForBinding:(id)binding
{
        //return [_NSBinder class];
        return [_NSKVOBinder class];
}

-(id)_binderForBinding:(id)binding;
{
        return [self _binderForBinding:binding create:NO];
}

-(id)_binderForBinding:(id)binding create:(BOOL)create
{
        if(!bindersForObjects)
                bindersForObjects=[NSMutableDictionary new];

        id key = [NSValue valueWithNonretainedObject:self];
        id ownBinders = [bindersForObjects objectForKey:key];

        if(!ownBinders)
        {
                ownBinders = [NSMutableDictionary dictionary];
                [bindersForObjects setObject:ownBinders forKey:key];
        }

        id binder=[ownBinders objectForKey:binding];

        if(!binder && create)
        {
                binder = [[[[self class] _binderClassForBinding:binding] new] autorelease];
                [ownBinders setObject:binder forKey:binding];
        }

        return binder;
}

-(id)_replacementKeyPathForBinding:(id)binding
{
        if([binding isEqual:@"value"]) {
                return @"objectValue";
        }
   // FIX: actually try and detect these
        if([binding hasPrefix:@"displayPatternValue"]) {
                NSBindingDebugLog(kNSBindingDebugLogLevel3, @"display pattern binding: %@ was unprocessed", binding);
                return @"objectValue";
        }

        return binding;
}

-(void)bind:(id)binding toObject:(id)destination withKeyPath:(NSString*)keyPath options:(NSDictionary*)options
{
        NSBindingDebugLog(kNSBindingDebugLogLevel1, @"binding: %@\n   toObject: %@\n   withKeyPath: %@\n   options: %@", binding, destination, keyPath, options);

        if(![[self class] _binderClassForBinding:binding]){
                NSBindingDebugLog(kNSBindingDebugLogLevel1, @"no binder class for binding: '%@'", binding);
                return;
    }

        id binder=[self _binderForBinding:binding create:NO];

        if(binder) {
                NSBindingDebugLog(kNSBindingDebugLogLevel2, @"unbinding binding: '%@' before rebinding", binding);
                [binder unbind];
        } else {
                NSBindingDebugLog(kNSBindingDebugLogLevel2, @"creating new binder for binding: '%@'", binding);
                binder=[self _binderForBinding:binding create:YES];
        }

        [binder setSource:self];
        [binder setDestination:destination];
        [binder setKeyPath:keyPath];
        [binder setBinding:binding];
        [binder setOptions:options];

        [binder bind];
}

-(void)unbind:(id)binding
{
        NSBindingDebugLog(kNSBindingDebugLogLevel1, @"binding: %@", binding);
        id key = [NSValue valueWithNonretainedObject:self];
        id ownBinders = [bindersForObjects objectForKey:key];

        id binder=[ownBinders objectForKey:binding];
        [binder unbind];

        [ownBinders removeObjectForKey:binding];
        if([ownBinders count]==0) {
                NSBindingDebugLog(kNSBindingDebugLogLevel2, @"Removing binders for key: %@", key);
                [bindersForObjects removeObjectForKey:key];
        }
}

-(void)_unbindAllBindings
{
        id key = [NSValue valueWithNonretainedObject:self];
        id ownBinders = [bindersForObjects objectForKey:key];
        id binder=nil;
        id en=[[ownBinders allValues] objectEnumerator];
        while((binder=[en nextObject]))
        {
                [binder unbind];
        }

        [bindersForObjects removeObjectForKey:key];
}

-(NSDictionary *)infoForBinding:(id)binding {
   _NSBinder    *binder=[self _binderForBinding:binding create:NO];
   NSDictionary *result=[NSDictionary dictionaryWithObjectsAndKeys:
      [binder destination],NSObservedObjectKey,
      [binder keyPath],NSObservedKeyPathKey,
      [binder options],NSOptionsKey,
      nil];

        return result;
}

+(void)exposeBinding:(id)binding
{

}

-(NSArray *)_allUsedBinders {
   NSValue      *key=[NSValue valueWithNonretainedObject:self];
   NSDictionary *ownBinders=[bindersForObjects objectForKey:key];

   return [ownBinders allValues];
}

@end
