/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSTableView.h>
#import <AppKit/NSDragging.h>
#import <AppKit/AppKitExport.h>

@class NSTableColumn, NSButtonCell;

/* The child index of a drop on an item itself. */
enum { NSOutlineViewDropOnItemIndex = -1 };

APPKIT_EXPORT NSString *const NSOutlineViewItemWillExpandNotification;
APPKIT_EXPORT NSString *const NSOutlineViewItemDidExpandNotification;
APPKIT_EXPORT NSString *const NSOutlineViewItemWillCollapseNotification;
APPKIT_EXPORT NSString *const NSOutlineViewItemDidCollapseNotification;

APPKIT_EXPORT NSString *const NSOutlineViewColumnDidMoveNotification;
APPKIT_EXPORT NSString *const NSOutlineViewColumnDidResizeNotification;

APPKIT_EXPORT NSString *const NSOutlineViewSelectionDidChangeNotification;
APPKIT_EXPORT NSString *const NSOutlineViewSelectionIsChangingNotification;

@interface NSOutlineView : NSTableView {
    NSTableColumn *_outlineTableColumn;
    NSMapTable *_rowToItem;
    NSMapTable *_itemToRow;
    NSMapTable *_itemToParent;
    NSMapTable *_itemToLevel;
    NSMapTable *_itemToExpansionState;
    NSMapTable *_itemToNumberOfChildren;

    unsigned _numberOfCachedRows;
    NSButtonCell *_markerCell;
    NSSize _markerSize;
    id _clickedItem;
    CGFloat _widestColumnWidth;

    CGFloat _indentationPerLevel;
    BOOL _indentationMarkerFollowsCell;
    BOOL _autoresizesOutlineColumn;
    BOOL _autosaveExpandedItems;
    CGFloat _editingCellPadding;
}

- (NSTableColumn *)outlineTableColumn;

- itemAtRow:(NSInteger)row;
- (NSInteger)rowForItem:item;
- parentForItem:item;

- (BOOL)isExpandable:item;
- (NSInteger)levelForItem:item;
- (NSInteger)levelForRow:(NSInteger)row;
- (BOOL)isItemExpanded:item;
- (CGFloat)indentationPerLevel;

- (BOOL)autoresizesOutlineColumn;
- (BOOL)indentationMarkerFollowsCell;
- (BOOL)autosaveExpandedItems;

- (void)setOutlineTableColumn:(NSTableColumn *)tableColumn;

- (void)setIndentationPerLevel:(CGFloat)value;
- (void)setAutoresizesOutlineColumn:(BOOL)flag;
- (void)setIndentationMarkerFollowsCell:(BOOL)flag;
- (void)setAutosaveExpandedItems:(BOOL)flag;

- (void)expandItem:item expandChildren:(BOOL)expandChildren;
- (void)expandItem:item;
- (void)collapseItem:item collapseChildren:(BOOL)collapseChildren;
- (void)collapseItem:item;
- (void)reloadItem:item reloadChildren:(BOOL)reloadChildren;
- (void)reloadItem:item;

- (void)setDropItem:item dropChildIndex:(NSInteger)index;
- (BOOL)shouldCollapseAutoExpandedItemsForDeposited:(BOOL)collapse;

- (NSRect)frameOfOutlineCellAtRow:(NSInteger)row;


- (void)insertItemsAtIndexes:(NSIndexSet *)indexes inParent:(id)parent withAnimation:(NSTableViewAnimationOptions)animationOptions;
- (void)removeItemsAtIndexes:(NSIndexSet *)indexes inParent:(id)parent withAnimation:(NSTableViewAnimationOptions)animationOptions;
- (void)moveItemAtIndex:(NSInteger)fromIndex inParent:(id)oldParent toIndex:(NSInteger)toIndex inParent:(id)newParent;

@end

@interface NSObject (NSOutlineView_dataSource)
- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:item;
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:item;
- outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:item;
- outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:item;
- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:object forTableColumn:(NSTableColumn *)tableColumn byItem:item;
@end

@interface NSObject (NSOutlineView_delegate)
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldExpandItem:item;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldCollapseItem:item;
- (BOOL)selectionShouldChangeInOutlineView:(NSOutlineView *)outlineView;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldEditTableColumn:(NSTableColumn *)tableColumn item:item;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:item;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectTableColumn:(NSTableColumn *)tableColumn;
- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:cell forTableColumn:(NSTableColumn *)tableColumn item:item;
- (void)outlineView:(NSOutlineView *)outlineView willDisplayOutlineCell:cell forTableColumn:(NSTableColumn *)tableColumn item:item;
@end

@interface NSObject (NSOutlineView_notifications)
- (void)outlineViewItemWillExpand:(NSNotification *)note;
- (void)outlineViewItemDidExpand:(NSNotification *)note;
- (void)outlineViewItemWillCollapse:(NSNotification *)note;
- (void)outlineViewItemDidCollapse:(NSNotification *)note;
- (void)outlineViewColumnDidMove:(NSNotification *)note;
- (void)outlineViewColumnDidResize:(NSNotification *)note;
- (void)outlineViewSelectionIsChanging:(NSNotification *)note;
- (void)outlineViewSelectionDidChange:(NSNotification *)note;
@end

@interface NSObject (NSOutlineView_cellSizing)
- (CGFloat)outlineView:(NSOutlineView *)outlineView widthOfCell:cell forTableColumn:(NSTableColumn *)tableColumn byItem:item;
@end

/* Minkar: the formal data source and delegate protocols (API 10.6), with
   the methods of the informal ones above and those of later APIs. */
@protocol NSOutlineViewDataSource <NSObject>
@optional
- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;
- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView itemForPersistentObject:(id)object;
- (id)outlineView:(NSOutlineView *)outlineView persistentObjectForItem:(id)item;
- (void)outlineView:(NSOutlineView *)outlineView sortDescriptorsDidChange:(NSArray *)oldDescriptors;
- (id<NSPasteboardWriting>)outlineView:(NSOutlineView *)outlineView pasteboardWriterForItem:(id)item;
- (void)outlineView:(NSOutlineView *)outlineView draggingSession:(NSDraggingSession *)session willBeginAtPoint:(NSPoint)screenPoint forItems:(NSArray *)draggedItems;
- (void)outlineView:(NSOutlineView *)outlineView draggingSession:(NSDraggingSession *)session endedAtPoint:(NSPoint)screenPoint operation:(NSDragOperation)operation;
- (BOOL)outlineView:(NSOutlineView *)outlineView writeItems:(NSArray *)items toPasteboard:(NSPasteboard *)pasteboard;
- (NSDragOperation)outlineView:(NSOutlineView *)outlineView validateDrop:(id<NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(NSInteger)index;
- (BOOL)outlineView:(NSOutlineView *)outlineView acceptDrop:(id<NSDraggingInfo>)info item:(id)item childIndex:(NSInteger)index;
@end

@protocol NSOutlineViewDelegate <NSControlTextEditingDelegate>
@optional
- (NSView *)outlineView:(NSOutlineView *)outlineView viewForTableColumn:(NSTableColumn *)tableColumn item:(id)item;
- (NSTableRowView *)outlineView:(NSOutlineView *)outlineView rowViewForItem:(id)item;
- (void)outlineView:(NSOutlineView *)outlineView didAddRowView:(NSTableRowView *)rowView forRow:(NSInteger)row;
- (void)outlineView:(NSOutlineView *)outlineView didRemoveRowView:(NSTableRowView *)rowView forRow:(NSInteger)row;
- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldEditTableColumn:(NSTableColumn *)tableColumn item:(id)item;
- (BOOL)selectionShouldChangeInOutlineView:(NSOutlineView *)outlineView;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item;
- (NSIndexSet *)outlineView:(NSOutlineView *)outlineView selectionIndexesForProposedSelection:(NSIndexSet *)proposedSelectionIndexes;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectTableColumn:(NSTableColumn *)tableColumn;
- (void)outlineView:(NSOutlineView *)outlineView mouseDownInHeaderOfTableColumn:(NSTableColumn *)tableColumn;
- (void)outlineView:(NSOutlineView *)outlineView didClickTableColumn:(NSTableColumn *)tableColumn;
- (void)outlineView:(NSOutlineView *)outlineView didDragTableColumn:(NSTableColumn *)tableColumn;
- (NSString *)outlineView:(NSOutlineView *)outlineView toolTipForCell:(NSCell *)cell rect:(NSRectPointer)rect tableColumn:(NSTableColumn *)tableColumn item:(id)item mouseLocation:(NSPoint)mouseLocation;
- (CGFloat)outlineView:(NSOutlineView *)outlineView heightOfRowByItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView isGroupItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldExpandItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldCollapseItem:(id)item;
- (void)outlineView:(NSOutlineView *)outlineView willDisplayOutlineCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item;
- (CGFloat)outlineView:(NSOutlineView *)outlineView sizeToFitWidthOfColumn:(NSInteger)column;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldReorderColumn:(NSInteger)columnIndex toColumn:(NSInteger)newColumnIndex;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldShowOutlineCellForItem:(id)item;
- (void)outlineViewSelectionDidChange:(NSNotification *)notification;
- (void)outlineViewColumnDidMove:(NSNotification *)notification;
- (void)outlineViewColumnDidResize:(NSNotification *)notification;
- (void)outlineViewSelectionIsChanging:(NSNotification *)notification;
- (void)outlineViewItemWillExpand:(NSNotification *)notification;
- (void)outlineViewItemDidExpand:(NSNotification *)notification;
- (void)outlineViewItemWillCollapse:(NSNotification *)notification;
- (void)outlineViewItemDidCollapse:(NSNotification *)notification;
@end
