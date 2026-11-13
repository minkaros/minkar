#import <AppKit/NSAccessibility.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <objc/runtime.h>

NSString * const NSAccessibilityChildrenAttribute=@"AXChildren";
NSString * const NSAccessibilityContentsAttribute=@"AXContents";
NSString * const NSAccessibilityDescriptionAttribute=@"AXDescription";
NSString * const NSAccessibilityEnabledAttribute=@"AXEnabled";
NSString * const NSAccessibilityFocusedAttribute=@"AXFocused";
NSString * const NSAccessibilityHelpAttribute=@"AXHelp";
NSString * const NSAccessibilityMaxValueAttribute=@"AXMaxValue";
NSString * const NSAccessibilityMinValueAttribute=@"AXMinValue";
NSString * const NSAccessibilityParentAttribute=@"AXParent";
NSString * const NSAccessibilityPositionAttribute=@"AXPosition";
NSString * const NSAccessibilityRoleAttribute=@"AXRole";
NSString * const NSAccessibilityRoleDescriptionAttribute=@"AXRoleDescription";
NSString * const NSAccessibilitySelectedChildrenAttribute=@"AXSelectedChildren";
NSString * const NSAccessibilityShownMenuAttribute=@"AXShownMenu";
NSString * const NSAccessibilitySizeAttribute=@"AXSize";
NSString * const NSAccessibilitySubroleAttribute=@"AXSubrole";
NSString * const NSAccessibilityTitleAttribute=@"AXTitle";
NSString * const NSAccessibilityTopLevelUIElementAttribute=@"AXTopLevelUIElement";
NSString * const NSAccessibilityValueAttribute=@"AXValue";
NSString * const NSAccessibilityValueDescriptionAttribute=@"AXValueDescription";
NSString * const NSAccessibilityVisibleChildrenAttribute=@"AXVisibleChildren";
NSString * const NSAccessibilityWindowAttribute=@"AXWindow";

NSString * const NSAccessibilityInsertionPointLineNumberAttribute=@"AXInsertionPointLineNumber";
NSString * const NSAccessibilityNumberOfCharactersAttribute=@"AXNumberOfCharacters";
NSString * const NSAccessibilitySelectedTextAttribute=@"AXSelectedText";
NSString * const NSAccessibilitySelectedTextRangeAttribute=@"AXSelectedTextRange";
NSString * const NSAccessibilitySelectedTextRangesAttribute=@"AXSelectedTextRanges";
NSString * const NSAccessibilitySharedCharacterRangeAttribute=@"AXSharedCharacterRange";
NSString * const NSAccessibilitySharedTextUIElementsAttribute=@"AXSharedTextUIElements";
NSString * const NSAccessibilityVisibleCharacterRangeAttribute=@"AXVisibleCharacterRange";

NSString * const NSAccessibilityAttributedStringForRangeParameterizedAttribute=@"AXAttributedStringForRange";
NSString * const NSAccessibilityBoundsForRangeParameterizedAttribute=@"AXBoundsForRange";
NSString * const NSAccessibilityLineForIndexParameterizedAttribute=@"AXLineForIndex";
NSString * const NSAccessibilityRTFForRangeParameterizedAttribute=@"AXRTFForRange";
NSString * const NSAccessibilityRangeForIndexParameterizedAttribute=@"AXRangeForIndex";
NSString * const NSAccessibilityRangeForLineParameterizedAttribute=@"AXRangeForLine";
NSString * const NSAccessibilityRangeForPositionParameterizedAttribute=@"AXRangeForPosition";
NSString * const NSAccessibilityStringForRangeParameterizedAttribute=@"AXStringForRange";
NSString * const NSAccessibilityStyleRangeForIndexParameterizedAttribute=@"AXStyleRangeForIndex";

NSString * const NSAccessibilityAttachmentTextAttribute=@"AXAttachmentText";
NSString * const NSAccessibilityBackgroundColorTextAttribute=@"AXBackgroundColorText";
NSString * const NSAccessibilityFontFamilyKey=@"NSAccessibilityFontFamilyKey";
NSString * const NSAccessibilityFontNameKey=@"NSAccessibilityFontNameKey";
NSString * const NSAccessibilityFontSizeKey=@"NSAccessibilityFontSizeKey";
NSString * const NSAccessibilityFontTextAttribute=@"AXFontText";
NSString * const NSAccessibilityForegroundColorTextAttribute=@"AXForegroundColorText";
NSString * const NSAccessibilityLinkTextAttribute=@"AXLinkText";
NSString * const NSAccessibilityMisspelledTextAttribute=@"AXMisspelledText";
NSString * const NSAccessibilityShadowTextAttribute=@"AXShadowText";
NSString * const NSAccessibilityStrikethroughColorTextAttribute=@"AXStrikethroughColorText";
NSString * const NSAccessibilityStrikethroughTextAttribute=@"AXStrikethroughText";
NSString * const NSAccessibilitySuperscriptTextAttribute=@"AXSuperscriptText";
NSString * const NSAccessibilityUnderlineColorTextAttribute=@"AXUnderlineColorText";
NSString * const NSAccessibilityUnderlineTextAttribute=@"AXUnderlineText";
NSString * const NSAccessibilityVisibleNameKey=@"NSAccessibilityVisibleNameKey";

NSString * const NSAccessibilityCancelButtonAttribute=@"AXCancelButton";
NSString * const NSAccessibilityCloseButtonAttribute=@"AXCloseButton";
NSString * const NSAccessibilityDefaultButtonAttribute=@"AXDefaultButton";
NSString * const NSAccessibilityGrowAreaAttribute=@"AXGrowArea";
NSString * const NSAccessibilityMainAttribute=@"AXMain";
NSString * const NSAccessibilityMinimizeButtonAttribute=@"AXMinimizeButton";
NSString * const NSAccessibilityMinimizedAttribute=@"AXMinimized";
NSString * const NSAccessibilityModalAttribute=@"AXModal";
NSString * const NSAccessibilityProxyAttribute=@"AXProxy";
NSString * const NSAccessibilityToolbarButtonAttribute=@"AXToolbarButton";
NSString * const NSAccessibilityZoomButtonAttribute=@"AXZoomButton";

NSString * const NSAccessibilityHorizontalOrientationValue=@"NSAccessibilityHorizontalOrientationValue";
NSString * const NSAccessibilityVerticalOrientationValue=@"NSAccessibilityVerticalOrientationValue";
NSString * const NSAccessibilityUnknownOrientationValue=@"NSAccessibilityUnknownOrientationValue";

NSString * const NSAccessibilityClearButtonAttribute=@"AXClearButton";
NSString * const NSAccessibilityColumnTitlesAttribute=@"AXColumnTitles";
NSString * const NSAccessibilityFocusedUIElementAttribute=@"AXFocusedUIElement";
NSString * const NSAccessibilityFocusedWindowAttribute=@"AXFocusedWindow";
NSString * const NSAccessibilityFrontmostAttribute=@"AXFrontmost";
NSString * const NSAccessibilityHiddenAttribute=@"AXHidden";
NSString * const NSAccessibilityMainWindowAttribute=@"AXMainWindow";
NSString * const NSAccessibilityMenuBarAttribute=@"AXMenuBar";
NSString * const NSAccessibilityOrientationAttribute=@"AXOrientation";
NSString * const NSAccessibilitySearchButtonAttribute=@"AXSearchButton";
NSString * const NSAccessibilitySearchMenuAttribute=@"AXSearchMenu";
NSString * const NSAccessibilityWindowsAttribute=@"AXWindows";

NSString * const NSAccessibilityColumnCountAttribute=@"AXColumnCount";
NSString * const NSAccessibilityOrderedByRowAttribute=@"AXOrderedByRow";
NSString * const NSAccessibilityRowCountAttribute=@"AXRowCount";

NSString * const NSAccessibilityColumnHeaderUIElementsAttribute=@"AXColumnHeaderUIElements";
NSString * const NSAccessibilityColumnsAttribute=@"AXColumns";
NSString * const NSAccessibilityRowHeaderUIElementsAttribute=@"AXRowHeaderUIElements";
NSString * const NSAccessibilityRowsAttribute=@"AXRows";
NSString * const NSAccessibilitySelectedColumnsAttribute=@"AXSelectedColumns";
NSString * const NSAccessibilitySelectedRowsAttribute=@"AXSelectedRows";
NSString * const NSAccessibilitySortDirectionAttribute=@"AXSortDirection";
NSString * const NSAccessibilityVisibleColumnsAttribute=@"AXVisibleColumns";
NSString * const NSAccessibilityVisibleRowsAttribute=@"AXVisibleRows";

NSString * const NSAccessibilityDisclosedByRowAttribute=@"AXDisclosedByRow";
NSString * const NSAccessibilityDisclosedRowsAttribute=@"AXDisclosedRows";
NSString * const NSAccessibilityDisclosingAttribute=@"AXDisclosing";
NSString * const NSAccessibilityDisclosureLevelAttribute=@"AXDisclosureLevel";

NSString * const NSAccessibilitySelectedCellsAttribute=@"AXSelectedCells";
NSString * const NSAccessibilityVisibleCellsAttribute=@"AXVisibleCells";

NSString * const NSAccessibilityCellForColumnAndRowParameterizedAttribute=@"AXCellForColumnAndRow";

NSString * const NSAccessibilityRowIndexRangeAttribute=@"AXRowIndexRange";
NSString * const NSAccessibilityColumnIndexRangeAttribute=@"AXColumnIndexRange";

NSString * const NSAccessibilityAscendingSortDirectionValue=@"NSAccessibilityAscendingSortDirectionValue";
NSString * const NSAccessibilityDescendingSortDirectionValue=@"NSAccessibilityDescendingSortDirectionValue";
NSString * const NSAccessibilityUnknownSortDirectionValue=@"NSAccessibilityUnknownSortDirectionValue";

NSString * const NSAccessibilityHorizontalUnitsAttribute=@"AXHorizontalUnits";
NSString * const NSAccessibilityVerticalUnitsAttribute=@"AXVerticalUnits";
NSString * const NSAccessibilityHorizontalUnitDescriptionAttribute=@"AXHorizontalUnitDescription";
NSString * const NSAccessibilityVerticalUnitDescriptionAttribute=@"AXVerticalUnitDescription";

NSString * const NSAccessibilityLayoutPointForScreenPointParameterizedAttribute=@"AXLayoutPointForScreenPoint";
NSString * const NSAccessibilityLayoutSizeForScreenSizeParameterizedAttribute=@"AXLayoutSizeForScreenSize";
NSString * const NSAccessibilityScreenPointForLayoutPointParameterizedAttribute=@"AXScreenPointForLayoutPoint";
NSString * const NSAccessibilityScreenSizeForLayoutSizeParameterizedAttribute=@"AXScreenSizeForLayoutSize";

NSString * const NSAccessibilityHandlesAttribute=@"AXHandles";

NSString * const NSAccessibilityAllowedValuesAttribute=@"AXAllowedValues";
NSString * const NSAccessibilityLabelUIElementsAttribute=@"AXLabelUIElements";
NSString * const NSAccessibilityLabelValueAttribute=@"AXLabelValue";

NSString * const NSAccessibilityMatteContentUIElementAttribute=@"AXMatteContentUIElement";
NSString * const NSAccessibilityMatteHoleAttribute=@"AXMatteHole";

NSString * const NSAccessibilityMarkerGroupUIElementAttribute=@"AXMarkerGroupUIElement";
NSString * const NSAccessibilityMarkerTypeAttribute=@"AXMarkerType";
NSString * const NSAccessibilityMarkerTypeDescriptionAttribute=@"AXMarkerTypeDescription";
NSString * const NSAccessibilityMarkerUIElementsAttribute=@"AXMarkerUIElements";
NSString * const NSAccessibilityMarkerValuesAttribute=@"AXMarkerValues";
NSString * const NSAccessibilityUnitDescriptionAttribute=@"AXUnitDescription";
NSString * const NSAccessibilityUnitsAttribute=@"AXUnits";

NSString * const NSAccessibilityCenterTabStopMarkerTypeValue=@"NSAccessibilityCenterTabStopMarkerTypeValue";
NSString * const NSAccessibilityDecimalTabStopMarkerTypeValue=@"NSAccessibilityDecimalTabStopMarkerTypeValue";
NSString * const NSAccessibilityFirstLineIndentMarkerTypeValue=@"NSAccessibilityFirstLineIndentMarkerTypeValue";
NSString * const NSAccessibilityHeadIndentMarkerTypeValue=@"NSAccessibilityHeadIndentMarkerTypeValue";
NSString * const NSAccessibilityLeftTabStopMarkerTypeValue=@"NSAccessibilityLeftTabStopMarkerTypeValue";
NSString * const NSAccessibilityRightTabStopMarkerTypeValue=@"NSAccessibilityRightTabStopMarkerTypeValue";
NSString * const NSAccessibilityTailIndentMarkerTypeValue=@"NSAccessibilityTailIndentMarkerTypeValue";
NSString * const NSAccessibilityUnknownMarkerTypeValue=@"NSAccessibilityUnknownMarkerTypeValue";

NSString * const NSAccessibilityCentimetersUnitValue=@"NSAccessibilityCentimetersUnitValue";
NSString * const NSAccessibilityInchesUnitValue=@"NSAccessibilityInchesUnitValue";
NSString * const NSAccessibilityPicasUnitValue=@"NSAccessibilityPicasUnitValue";
NSString * const NSAccessibilityPointsUnitValue=@"NSAccessibilityPointsUnitValue";
NSString * const NSAccessibilityUnknownUnitValue=@"NSAccessibilityUnknownUnitValue";

NSString * const NSAccessibilityLinkedUIElementsAttribute=@"AXLinkedUIElements";
NSString * const NSAccessibilityServesAsTitleForUIElementsAttribute=@"AXServesAsTitleForUIElements";
NSString * const NSAccessibilityTitleUIElementAttribute=@"AXTitleUIElement";

NSString * const NSAccessibilityDecrementButtonAttribute=@"AXDecrementButton";
NSString * const NSAccessibilityDocumentAttribute=@"AXDocument";
NSString * const NSAccessibilityEditedAttribute=@"AXEdited";
NSString * const NSAccessibilityExpandedAttribute=@"AXExpanded";
NSString * const NSAccessibilityFilenameAttribute=@"AXFilename";
NSString * const NSAccessibilityHeaderAttribute=@"AXHeader";
NSString * const NSAccessibilityHorizontalScrollBarAttribute=@"AXHorizontalScrollBar";
NSString * const NSAccessibilityIncrementButtonAttribute=@"AXIncrementButton";
NSString * const NSAccessibilityIndexAttribute=@"AXIndex";
NSString * const NSAccessibilityNextContentsAttribute=@"AXNextContents";
NSString * const NSAccessibilityOverflowButtonAttribute=@"AXOverflowButton";
NSString * const NSAccessibilityPreviousContentsAttribute=@"AXPreviousContents";
NSString * const NSAccessibilitySelectedAttribute=@"AXSelected";
NSString * const NSAccessibilitySplittersAttribute=@"AXSplitters";
NSString * const NSAccessibilityTabsAttribute=@"AXTabs";
NSString * const NSAccessibilityURLAttribute=@"AXURL";
NSString * const NSAccessibilityVerticalScrollBarAttribute=@"AXVerticalScrollBar";
NSString * const NSAccessibilityWarningValueAttribute=@"AXWarningValue";
NSString * const NSAccessibilityCriticalValueAttribute=@"AXCriticalValue";
NSString * const NSAccessibilityPlaceholderValueAttribute=@"AXPlaceholderValue";
NSString * const NSAccessibilityMarkedMisspelledTextAttribute=@"AXMarkedMisspelledText";

NSString * const NSAccessibilityApplicationRole=@"AXApplication";
NSString * const NSAccessibilityBrowserRole=@"AXBrowser";
NSString * const NSAccessibilityBusyIndicatorRole=@"AXBusyIndicator";
NSString * const NSAccessibilityButtonRole=@"AXButton";
NSString * const NSAccessibilityLevelIndicatorRole=@"AXLevelIndicator";
NSString * const NSAccessibilityPopoverRole=@"AXPopover";
NSString * const NSAccessibilityCellRole=@"AXCell";
NSString * const NSAccessibilityCheckBoxRole=@"AXCheckBox";
NSString * const NSAccessibilityColorWellRole=@"AXColorWell";
NSString * const NSAccessibilityColumnRole=@"AXColumn";
NSString * const NSAccessibilityComboBoxRole=@"AXComboBox";
NSString * const NSAccessibilityDisclosureTriangleRole=@"AXDisclosureTriangle";
NSString * const NSAccessibilityDrawerRole=@"AXDrawer";
NSString * const NSAccessibilityGridRole=@"AXGrid";
NSString * const NSAccessibilityGroupRole=@"AXGroup";
NSString * const NSAccessibilityGrowAreaRole=@"AXGrowArea";
NSString * const NSAccessibilityHandleRole=@"AXHandle";
NSString * const NSAccessibilityHelpTagRole=@"AXHelpTag";
NSString * const NSAccessibilityImageRole=@"AXImage";
NSString * const NSAccessibilityIncrementorRole=@"AXIncrementor";
NSString * const NSAccessibilityLayoutAreaRole=@"AXLayoutArea";
NSString * const NSAccessibilityLayoutItemRole=@"AXLayoutItem";
NSString * const NSAccessibilityLinkRole=@"AXLink";
NSString * const NSAccessibilityListRole=@"AXList";
NSString * const NSAccessibilityMatteRole=@"AXMatte";
NSString * const NSAccessibilityMenuBarRole=@"AXMenuBar";
NSString * const NSAccessibilityMenuButtonRole=@"AXMenuButton";
NSString * const NSAccessibilityMenuItemRole=@"AXMenuItem";
NSString * const NSAccessibilityMenuRole=@"AXMenu";
NSString * const NSAccessibilityOutlineRole=@"AXOutline";
NSString * const NSAccessibilityPopUpButtonRole=@"AXPopUpButton";
NSString * const NSAccessibilityProgressIndicatorRole=@"AXProgressIndicator";
NSString * const NSAccessibilityRadioButtonRole=@"AXRadioButton";
NSString * const NSAccessibilityRadioGroupRole=@"AXRadioGroup";
NSString * const NSAccessibilityRelevanceIndicatorRole=@"AXRelevanceIndicator";
NSString * const NSAccessibilityRowRole=@"AXRow";
NSString * const NSAccessibilityRulerMarkerRole=@"AXRulerMarker";
NSString * const NSAccessibilityRulerRole=@"AXRuler";
NSString * const NSAccessibilityScrollAreaRole=@"AXScrollArea";
NSString * const NSAccessibilityScrollBarRole=@"AXScrollBar";
NSString * const NSAccessibilitySheetRole=@"AXSheet";
NSString * const NSAccessibilitySliderRole=@"AXSlider";
NSString * const NSAccessibilitySortButtonRole=@"AXSortButton";
NSString * const NSAccessibilitySplitGroupRole=@"AXSplitGroup";
NSString * const NSAccessibilitySplitterRole=@"AXSplitter";
NSString * const NSAccessibilityStaticTextRole=@"AXStaticText";
NSString * const NSAccessibilitySystemWideRole=@"AXSystemWide";
NSString * const NSAccessibilityTabGroupRole=@"AXTabGroup";
NSString * const NSAccessibilityTableRole=@"AXTable";
NSString * const NSAccessibilityTextAreaRole=@"AXTextArea";
NSString * const NSAccessibilityTextFieldRole=@"AXTextField";
NSString * const NSAccessibilityToolbarRole=@"AXToolbar";
NSString * const NSAccessibilityUnknownRole=@"AXUnknown";
NSString * const NSAccessibilityValueIndicatorRole=@"AXValueIndicator";
NSString * const NSAccessibilityWindowRole=@"AXWindow";

NSString * const NSAccessibilityCloseButtonSubrole=@"AXCloseButton";
NSString * const NSAccessibilityDecrementArrowSubrole=@"AXDecrementArrow";
NSString * const NSAccessibilityDecrementPageSubrole=@"AXDecrementPage";
NSString * const NSAccessibilityDialogSubrole=@"AXDialog";
NSString * const NSAccessibilityFloatingWindowSubrole=@"AXFloatingWindow";
NSString * const NSAccessibilityIncrementArrowSubrole=@"AXIncrementArrow";
NSString * const NSAccessibilityIncrementPageSubrole=@"AXIncrementPage";
NSString * const NSAccessibilityMinimizeButtonSubrole=@"AXMinimizeButton";
NSString * const NSAccessibilityOutlineRowSubrole=@"AXOutlineRow";
NSString * const NSAccessibilitySearchFieldSubrole=@"AXSearchField";
NSString * const NSAccessibilitySecureTextFieldSubrole=@"AXSecureTextField";
NSString * const NSAccessibilityStandardWindowSubrole=@"AXStandardWindow";
NSString * const NSAccessibilitySystemDialogSubrole=@"AXSystemDialog";
NSString * const NSAccessibilitySystemFloatingWindowSubrole=@"AXSystemFloatingWindow";
NSString * const NSAccessibilityTableRowSubrole=@"AXTableRow";
NSString * const NSAccessibilityTextAttachmentSubrole=@"AXTextAttachment";
NSString * const NSAccessibilityTextLinkSubrole=@"AXTextLink";
NSString * const NSAccessibilityTimelineSubrole=@"AXTimeline";
NSString * const NSAccessibilityToolbarButtonSubrole=@"AXToolbarButton";
NSString * const NSAccessibilityUnknownSubrole=@"AXUnknown";
NSString * const NSAccessibilityZoomButtonSubrole=@"AXZoomButton";
NSString * const NSAccessibilitySortButtonSubrole=@"AXSortButton";
NSString * const NSAccessibilityRatingIndicatorSubrole=@"AXRatingIndicator";
NSString * const NSAccessibilityContentListSubrole=@"AXContentList";
NSString * const NSAccessibilityDefinitionListSubrole=@"AXDefinitionList";
NSString * const NSAccessibilitySwitchSubrole=@"AXSwitch";
NSString * const NSAccessibilityToggleSubrole=@"AXToggle";


NSString * const NSAccessibilityCancelAction=@"AXCancel";
NSString * const NSAccessibilityConfirmAction=@"AXConfirm";
NSString * const NSAccessibilityDecrementAction=@"AXDecrement";
NSString * const NSAccessibilityDeleteAction=@"AXDelete";
NSString * const NSAccessibilityIncrementAction=@"AXIncrement";
NSString * const NSAccessibilityPickAction=@"AXPick";
NSString * const NSAccessibilityPressAction=@"AXPress";
NSString * const NSAccessibilityRaiseAction=@"AXRaise";
NSString * const NSAccessibilityShowMenuAction=@"AXShowMenu";

NSString * const NSAccessibilityMainWindowChangedNotification=@"AXMainWindowChanged";
NSString * const NSAccessibilityFocusedWindowChangedNotification=@"AXFocusedWindowChanged";
NSString * const NSAccessibilityFocusedUIElementChangedNotification=@"AXFocusedUIElementChanged";

NSString * const NSAccessibilityWindowCreatedNotification=@"AXWindowCreated";
NSString * const NSAccessibilityWindowDeminiaturizedNotification=@"AXWindowDeminiaturized";
NSString * const NSAccessibilityWindowMiniaturizedNotification=@"AXWindowMiniaturized";
NSString * const NSAccessibilityWindowMovedNotification=@"AXWindowMoved";
NSString * const NSAccessibilityWindowResizedNotification=@"AXWindowResized";

NSString * const NSAccessibilityApplicationActivatedNotification=@"AXApplicationActivated";
NSString * const NSAccessibilityApplicationDeactivatedNotification=@"AXApplicationDeactivated";
NSString * const NSAccessibilityApplicationHiddenNotification=@"AXApplicationHidden";
NSString * const NSAccessibilityApplicationShownNotification=@"AXApplicationShown";

NSString * const NSAccessibilityDrawerCreatedNotification=@"AXDrawerCreated";
NSString * const NSAccessibilitySheetCreatedNotification=@"AXSheetCreated";

NSString * const NSAccessibilityCreatedNotification=@"AXCreated";
NSString * const NSAccessibilityMovedNotification=@"AXMoved";
NSString * const NSAccessibilityResizedNotification=@"AXResized";
NSString * const NSAccessibilityTitleChangedNotification=@"AXTitleChanged";
NSString * const NSAccessibilityUIElementDestroyedNotification=@"AXUIElementDestroyed";
NSString * const NSAccessibilityValueChangedNotification=@"AXValueChanged";

NSString * const NSAccessibilityHelpTagCreatedNotification=@"AXHelpTagCreated";
NSString * const NSAccessibilityRowCountChangedNotification=@"AXRowCountChanged";
NSString * const NSAccessibilitySelectedChildrenChangedNotification=@"AXSelectedChildrenChanged";
NSString * const NSAccessibilitySelectedColumnsChangedNotification=@"AXSelectedColumnsChanged";
NSString * const NSAccessibilitySelectedRowsChangedNotification=@"AXSelectedRowsChanged";
NSString * const NSAccessibilitySelectedTextChangedNotification=@"AXSelectedTextChanged";
NSString * const NSAccessibilityRowExpandedNotification=@"AXRowExpanded";
NSString * const NSAccessibilityRowCollapsedNotification=@"AXRowCollapsed";
NSString * const NSAccessibilitySelectedCellsChangedNotification=@"AXSelectedCellsChanged";
NSString * const NSAccessibilityUnitsChangedNotification=@"AXUnitsChanged";
NSString * const NSAccessibilitySelectedChildrenMovedNotification=@"AXSelectedChildrenMoved";

NSString * const NSAccessibilityErrorCodeExceptionInfo=@"NSAccessibilityErrorCodeExceptionInfo";

NSString * const NSAccessibilityException=@"NSAccessibilityException";
NSString * const NSAccessibilityAnnouncementRequestedNotification=@"AXAnnouncementRequested";
NSString * const NSAccessibilityAnnouncementKey=@"AXAnnouncementKey";
NSString * const NSAccessibilityPriorityKey=@"AXPriorityKey";
NSString * const NSAccessibilityLayoutChangedNotification=@"AXLayoutChanged";

/* No assistive application listens yet. */
void NSAccessibilityPostNotification(id element,NSString *notification) {
}

void NSAccessibilityPostNotificationWithUserInfo(id element,NSString *notification,NSDictionary *userInfo) {
}

/* What a role is called: its name without the AX prefix, in words
   ("AXPopUpButton": "pop up button"), and the subrole's when there is
   one. */
static NSString *wordsOfName(NSString *name) {
   NSMutableString *result=[NSMutableString string];
   NSUInteger       i,length;

   if([name hasPrefix:@"AX"])
    name=[name substringFromIndex:2];
   length=[name length];
   for(i=0;i<length;i++){
    unichar c=[name characterAtIndex:i];

    if(i>0 && c>='A' && c<='Z')
     [result appendString:@" "];
    [result appendFormat:@"%C",(unichar)((c>='A' && c<='Z')?c+('a'-'A'):c)];
   }
   return result;
}

NSString *NSAccessibilityRoleDescription(NSString *role,NSString *subrole) {
   if([subrole length]>0)
    return wordsOfName(subrole);
   return ([role length]>0)?wordsOfName(role):nil;
}

NSString *NSAccessibilityRoleDescriptionForUIElement(id element) {
   NSString *role=[element respondsToSelector:@selector(accessibilityRole)]?[element accessibilityRole]:nil;
   NSString *subrole=[element respondsToSelector:@selector(accessibilitySubrole)]?[element accessibilitySubrole]:nil;

   if(role==nil && [element respondsToSelector:@selector(accessibilityAttributeValue:)])
    role=[element accessibilityAttributeValue:NSAccessibilityRoleAttribute];
   return NSAccessibilityRoleDescription(role,subrole);
}

NSString *NSAccessibilityActionDescription(NSString *action) {
   return ([action length]>0)?wordsOfName(action):nil;
}

/* The element itself, or the first ancestor that is not ignored. */
id NSAccessibilityUnignoredAncestor(id element) {
   while(element!=nil){
    BOOL ignored=[element respondsToSelector:@selector(accessibilityIsIgnored)] && [element accessibilityIsIgnored];

    if(!ignored)
     return element;
    if([element respondsToSelector:@selector(superview)])
     element=[element superview];
    else
     return nil;
   }
   return nil;
}

NSRect NSAccessibilityFrameInView(NSView *parentView,NSRect frame) {
   NSWindow *window=[parentView window];
   NSRect    inWindow=[parentView convertRect:frame toView:nil];

   if(window==nil)
    return inWindow;
   inWindow.origin=[window convertBaseToScreen:inWindow.origin];
   return inWindow;
}

NSPoint NSAccessibilityPointInView(NSView *parentView,NSPoint point) {
   NSWindow *window=[parentView window];
   NSPoint   inWindow=[parentView convertPoint:point toView:nil];

   return (window!=nil)?[window convertBaseToScreen:inWindow]:inWindow;
}

void NSAccessibilityRaiseBadArgumentException(id element,NSString *attribute,id value) {
   [NSException raise:NSAccessibilityException format:@"%@: bad value %@ for %@",element,value,attribute];
}

@implementation NSAccessibilityCustomRotorSearchParameters
-(void)dealloc { [_currentItem release]; [_filterString release]; [super dealloc]; }
-(NSAccessibilityCustomRotorItemResult *)currentItem { return _currentItem; }
-(void)setCurrentItem:(NSAccessibilityCustomRotorItemResult *)item { item=[item retain]; [_currentItem release]; _currentItem=item; }
-(NSAccessibilityCustomRotorSearchDirection)searchDirection { return _searchDirection; }
-(void)setSearchDirection:(NSAccessibilityCustomRotorSearchDirection)value { _searchDirection=value; }
-(NSString *)filterString { return _filterString; }
-(void)setFilterString:(NSString *)value { value=[value copy]; [_filterString release]; _filterString=value; }
@end

@implementation NSAccessibilityCustomRotor
-initWithLabel:(NSString *)label itemSearchDelegate:(id)itemSearchDelegate {
   _label=[label copy];
   _itemSearchDelegate=itemSearchDelegate;
   return self;
}
-initWithRotorType:(NSAccessibilityCustomRotorType)rotorType itemSearchDelegate:(id)itemSearchDelegate {
   _type=rotorType;
   _itemSearchDelegate=itemSearchDelegate;
   return self;
}
-(void)dealloc { [_label release]; [super dealloc]; }
-(NSAccessibilityCustomRotorType)type { return _type; }
-(void)setType:(NSAccessibilityCustomRotorType)value { _type=value; }
-(NSString *)label { return _label; }
-(void)setLabel:(NSString *)value { value=[value copy]; [_label release]; _label=value; }
-(id)itemSearchDelegate { return _itemSearchDelegate; }
-(void)setItemSearchDelegate:(id)value { _itemSearchDelegate=value; }
-(id)itemLoadingDelegate { return _itemLoadingDelegate; }
-(void)setItemLoadingDelegate:(id)value { _itemLoadingDelegate=value; }
@end

@implementation NSAccessibilityCustomRotorItemResult
-initWithTargetElement:(id)targetElement {
   _targetElement=targetElement;
   _targetRange=NSMakeRange(NSNotFound,0);
   return self;
}
-initWithItemLoadingToken:(id)itemLoadingToken customLabel:(NSString *)customLabel {
   _itemLoadingToken=[itemLoadingToken retain];
   _customLabel=[customLabel copy];
   _targetRange=NSMakeRange(NSNotFound,0);
   return self;
}
-(void)dealloc { [_itemLoadingToken release]; [_customLabel release]; [super dealloc]; }
-(id)targetElement { return _targetElement; }
-(id)itemLoadingToken { return _itemLoadingToken; }
-(NSRange)targetRange { return _targetRange; }
-(void)setTargetRange:(NSRange)value { _targetRange=value; }
-(NSString *)customLabel { return _customLabel; }
-(void)setCustomLabel:(NSString *)value { value=[value copy]; [_customLabel release]; _customLabel=value; }
@end

/* The properties are kept with the object, for an assistive application
   to read. */
static char accessibilityKey;

static NSMutableDictionary *accessibilityValues(id object) {
   NSMutableDictionary *values=objc_getAssociatedObject(object,&accessibilityKey);

   if(values==nil){
    values=[NSMutableDictionary dictionary];
    objc_setAssociatedObject(object,&accessibilityKey,values,OBJC_ASSOCIATION_RETAIN);
   }
   return values;
}

static id accessibilityGet(id object,NSString *key) {
   id value=[objc_getAssociatedObject(object,&accessibilityKey) objectForKey:key];

   return (value==[NSNull null])?nil:value;
}

static void accessibilitySet(id object,NSString *key,id value) {
   if(value==nil)
    [accessibilityValues(object) removeObjectForKey:key];
   else
    [accessibilityValues(object) setObject:value forKey:key];
}

#define AX_OBJECT(getter,setter,key) \
-(id)getter { return accessibilityGet(self,key); } \
-(void)setter:(id)value { accessibilitySet(self,key,value); }

#define AX_WEAK(getter,setter,key) \
-(id)getter { return [accessibilityGet(self,key) nonretainedObjectValue]; } \
-(void)setter:(id)value { accessibilitySet(self,key,(value!=nil)?[NSValue valueWithNonretainedObject:value]:nil); }

@implementation NSObject (NSAccessibilityProperties)

-(BOOL)isAccessibilityElement { return [accessibilityGet(self,@"element") boolValue]; }
-(void)setAccessibilityElement:(BOOL)flag { accessibilitySet(self,@"element",[NSNumber numberWithBool:flag]); }

AX_OBJECT(accessibilityLabel,setAccessibilityLabel,@"label")
AX_OBJECT(accessibilityTitle,setAccessibilityTitle,@"title")
AX_OBJECT(accessibilityHelp,setAccessibilityHelp,@"help")
AX_OBJECT(accessibilityRole,setAccessibilityRole,@"role")
AX_OBJECT(accessibilitySubrole,setAccessibilitySubrole,@"subrole")
AX_OBJECT(accessibilityIdentifier,setAccessibilityIdentifier,@"identifier")
AX_OBJECT(accessibilityValue,setAccessibilityValue,@"value")
AX_OBJECT(accessibilityValueDescription,setAccessibilityValueDescription,@"valueDescription")
AX_OBJECT(accessibilityChildren,setAccessibilityChildren,@"children")
AX_OBJECT(accessibilitySharedFocusElements,setAccessibilitySharedFocusElements,@"sharedFocusElements")
AX_OBJECT(accessibilityCustomRotors,setAccessibilityCustomRotors,@"customRotors")
AX_WEAK(accessibilityTitleUIElement,setAccessibilityTitleUIElement,@"titleUIElement")
AX_WEAK(accessibilityParent,setAccessibilityParent,@"parent")

/* Its own, or the role's description. */
-(NSString *)accessibilityRoleDescription {
   NSString *own=accessibilityGet(self,@"roleDescription");

   return (own!=nil)?own:NSAccessibilityRoleDescription([self accessibilityRole],[self accessibilitySubrole]);
}

-(void)setAccessibilityRoleDescription:(NSString *)value {
   accessibilitySet(self,@"roleDescription",value);
}

/* A view's frame on screen, unless set. */
-(NSRect)accessibilityFrame {
   NSValue *own=accessibilityGet(self,@"frame");

   if(own!=nil)
    return [own rectValue];
   if([self isKindOfClass:[NSView class]])
    return NSAccessibilityFrameInView((NSView *)self,[(NSView *)self bounds]);
   return NSZeroRect;
}

-(void)setAccessibilityFrame:(NSRect)frame {
   accessibilitySet(self,@"frame",[NSValue valueWithRect:frame]);
}

-(BOOL)isAccessibilityEnabled {
   id own=accessibilityGet(self,@"enabled");

   return (own!=nil)?[own boolValue]:YES;
}

-(void)setAccessibilityEnabled:(BOOL)flag {
   accessibilitySet(self,@"enabled",[NSNumber numberWithBool:flag]);
}

-(BOOL)isAccessibilityFocused {
   return [accessibilityGet(self,@"focused") boolValue];
}

-(void)setAccessibilityFocused:(BOOL)flag {
   accessibilitySet(self,@"focused",[NSNumber numberWithBool:flag]);
}

@end

/* Minkar: the older API, as the reference platform maps it onto the
   properties (it was only declared). */
@implementation NSObject (NSAccessibility)

-(NSArray *)accessibilityAttributeNames {
   return [NSArray arrayWithObjects:NSAccessibilityRoleAttribute,NSAccessibilityRoleDescriptionAttribute,
      NSAccessibilitySubroleAttribute,NSAccessibilityTitleAttribute,NSAccessibilityValueAttribute,
      NSAccessibilityHelpAttribute,NSAccessibilityParentAttribute,NSAccessibilityChildrenAttribute,
      NSAccessibilityEnabledAttribute,NSAccessibilityFocusedAttribute,NSAccessibilityPositionAttribute,
      NSAccessibilitySizeAttribute,nil];
}

-accessibilityAttributeValue:(NSString *)attribute {
   if([attribute isEqualToString:NSAccessibilityRoleAttribute]) return [self accessibilityRole];
   if([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute]) return [self accessibilityRoleDescription];
   if([attribute isEqualToString:NSAccessibilitySubroleAttribute]) return [self accessibilitySubrole];
   if([attribute isEqualToString:NSAccessibilityTitleAttribute]) return [self accessibilityTitle];
   if([attribute isEqualToString:NSAccessibilityValueAttribute]) return [self accessibilityValue];
   if([attribute isEqualToString:NSAccessibilityHelpAttribute]) return [self accessibilityHelp];
   if([attribute isEqualToString:NSAccessibilityDescriptionAttribute]) return [self accessibilityLabel];
   if([attribute isEqualToString:NSAccessibilityParentAttribute]) return [self accessibilityParent];
   if([attribute isEqualToString:NSAccessibilityChildrenAttribute]) return [self accessibilityChildren];
   if([attribute isEqualToString:NSAccessibilityEnabledAttribute]) return [NSNumber numberWithBool:[self isAccessibilityEnabled]];
   if([attribute isEqualToString:NSAccessibilityFocusedAttribute]) return [NSNumber numberWithBool:[self isAccessibilityFocused]];
   if([attribute isEqualToString:NSAccessibilityPositionAttribute]) return [NSValue valueWithPoint:[self accessibilityFrame].origin];
   if([attribute isEqualToString:NSAccessibilitySizeAttribute]) return [NSValue valueWithSize:[self accessibilityFrame].size];
   return nil;
}

-(BOOL)accessibilityIsAttributeSettable:(NSString *)attribute {
   return [attribute isEqualToString:NSAccessibilityFocusedAttribute];
}

-(void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute {
   if([attribute isEqualToString:NSAccessibilityFocusedAttribute])
    [self setAccessibilityFocused:[value boolValue]];
}

-(NSArray *)accessibilityParameterizedAttributeNames { return [NSArray array]; }
-accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter { return nil; }
-(NSArray *)accessibilityActionNames { return [NSArray array]; }
-(NSString *)accessibilityActionDescription:(NSString *)action { return nil; }
-(void)accessibilityPerformAction:(NSString *)action { }
-(BOOL)accessibilityIsIgnored { return NO; }
-accessibilityHitTest:(NSPoint)point { return self; }
-accessibilityFocusedUIElement { return self; }

-(NSUInteger)accessibilityIndexOfChild:(id)child {
   return [[self accessibilityChildren] indexOfObjectIdenticalTo:child];
}

-(NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute {
   id value=[self accessibilityAttributeValue:attribute];
   return [value isKindOfClass:[NSArray class]]?[value count]:0;
}

-(NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount {
   id value=[self accessibilityAttributeValue:attribute];
   if(![value isKindOfClass:[NSArray class]] || index>=[value count])
    return [NSArray array];
   return [value subarrayWithRange:NSMakeRange(index,MIN(maxCount,[value count]-index))];
}

@end
