/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSGridView.h>
#import <AppKit/NSLayoutConstraint.h>
#import <AppKit/NSLayoutGuide.h>
#include <float.h>

const CGFloat NSGridViewSizeForContent=FLT_MIN;

@interface NSGridRow (Grid)
-(void)_setGridView:(NSGridView *)view;
@end
@interface NSGridColumn (Grid)
-(void)_setGridView:(NSGridView *)view;
@end
@interface NSGridCell (Grid)
-(void)_setGridView:(NSGridView *)view row:(NSGridRow *)row column:(NSGridColumn *)column;
-(NSGridCell *)_head;
-(void)_setHead:(NSGridCell *)head;
@end
@interface NSGridView (Grid)
-(void)_relayout;
-(NSGridCell *)_cellAtColumn:(NSInteger)column row:(NSInteger)row;
@end

/* A view with no width of its own and nothing inside to give it one (an
   editable text field): it fills its cell, and its column takes the
   grid's spare width. */
static BOOL stretchesAcross(NSView *view){
   return view!=nil && [view intrinsicContentSize].width==NSViewNoIntrinsicMetric && [[view constraints] count]==0;
}

/* The placeholder of an empty cell, never a subview. */
static BOOL isContent(NSView *view){
   return view!=nil && view!=[NSGridCell emptyContentView];
}

@implementation NSGridRow

-(void)_setGridView:(NSGridView *)view { _gridView=view; }
-(NSGridView *)gridView { return _gridView; }

-(NSInteger)numberOfCells {
   return [_gridView numberOfColumns];
}

-(NSGridCell *)cellAtIndex:(NSInteger)index {
   return [_gridView cellAtColumnIndex:index rowIndex:[_gridView indexOfRow:self]];
}

-(NSGridCellPlacement)yPlacement { return _yPlacement; }
-(void)setYPlacement:(NSGridCellPlacement)value { _yPlacement=value; [_gridView _relayout]; }
-(NSGridRowAlignment)rowAlignment { return _rowAlignment; }
-(void)setRowAlignment:(NSGridRowAlignment)value { _rowAlignment=value; [_gridView _relayout]; }
-(CGFloat)height { return _height; }
-(void)setHeight:(CGFloat)value { _height=value; [_gridView _relayout]; }
-(CGFloat)topPadding { return _topPadding; }
-(void)setTopPadding:(CGFloat)value { _topPadding=value; [_gridView _relayout]; }
-(CGFloat)bottomPadding { return _bottomPadding; }
-(void)setBottomPadding:(CGFloat)value { _bottomPadding=value; [_gridView _relayout]; }
-(BOOL)isHidden { return _hidden; }
-(void)setHidden:(BOOL)flag { _hidden=flag; [_gridView _relayout]; }

-(void)mergeCellsInRange:(NSRange)range {
   NSInteger index=[_gridView indexOfRow:self];

   [_gridView mergeCellsInHorizontalRange:range verticalRange:NSMakeRange(index,1)];
}

-initWithCoder:(NSCoder *)coder { return [self init]; }
-(void)encodeWithCoder:(NSCoder *)coder { }

@end

@implementation NSGridColumn

-(void)_setGridView:(NSGridView *)view { _gridView=view; }
-(NSGridView *)gridView { return _gridView; }

-(NSInteger)numberOfCells {
   return [_gridView numberOfRows];
}

-(NSGridCell *)cellAtIndex:(NSInteger)index {
   return [_gridView cellAtColumnIndex:[_gridView indexOfColumn:self] rowIndex:index];
}

-(NSGridCellPlacement)xPlacement { return _xPlacement; }
-(void)setXPlacement:(NSGridCellPlacement)value { _xPlacement=value; [_gridView _relayout]; }
-(CGFloat)width { return _width; }
-(void)setWidth:(CGFloat)value { _width=value; [_gridView _relayout]; }
-(CGFloat)leadingPadding { return _leadingPadding; }
-(void)setLeadingPadding:(CGFloat)value { _leadingPadding=value; [_gridView _relayout]; }
-(CGFloat)trailingPadding { return _trailingPadding; }
-(void)setTrailingPadding:(CGFloat)value { _trailingPadding=value; [_gridView _relayout]; }
-(BOOL)isHidden { return _hidden; }
-(void)setHidden:(BOOL)flag { _hidden=flag; [_gridView _relayout]; }

-(void)mergeCellsInRange:(NSRange)range {
   NSInteger index=[_gridView indexOfColumn:self];

   [_gridView mergeCellsInHorizontalRange:NSMakeRange(index,1) verticalRange:range];
}

-initWithCoder:(NSCoder *)coder { return [self init]; }
-(void)encodeWithCoder:(NSCoder *)coder { }

@end

@implementation NSGridCell

+(NSView *)emptyContentView {
   static NSView *empty;

   if(empty==nil)
    empty=[[NSView alloc] initWithFrame:NSZeroRect];
   return empty;
}

-(void)dealloc {
   [_contentView release];
   [_customPlacementConstraints release];
   [super dealloc];
}

-(void)_setGridView:(NSGridView *)view row:(NSGridRow *)row column:(NSGridColumn *)column {
   _gridView=view;
   _row=row;
   _column=column;
}

-(NSGridCell *)_head { return _headOfMergedCell; }
-(void)_setHead:(NSGridCell *)head { _headOfMergedCell=head; }

-(NSView *)contentView { return _contentView; }

-(void)setContentView:(NSView *)view {
   if(view==_contentView)
    return;
   if(isContent(_contentView))
    [_contentView removeFromSuperview];
   view=[view retain];
   [_contentView release];
   _contentView=view;
   // As on the reference platform, the grid places its views by
   // constraints (an app's own constraints between them then hold).
   if(isContent(view)){
    [view setTranslatesAutoresizingMaskIntoConstraints:NO];
    if(_gridView!=nil)
     [_gridView addSubview:view];
   }
   [_gridView _relayout];
}

-(NSGridRow *)row { return _row; }
-(NSGridColumn *)column { return _column; }
-(NSGridCellPlacement)xPlacement { return _xPlacement; }
-(void)setXPlacement:(NSGridCellPlacement)value { _xPlacement=value; [_gridView _relayout]; }
-(NSGridCellPlacement)yPlacement { return _yPlacement; }
-(void)setYPlacement:(NSGridCellPlacement)value { _yPlacement=value; [_gridView _relayout]; }
-(NSGridRowAlignment)rowAlignment { return _rowAlignment; }
-(void)setRowAlignment:(NSGridRowAlignment)value { _rowAlignment=value; [_gridView _relayout]; }
-(NSArray *)customPlacementConstraints { return _customPlacementConstraints; }
-(void)setCustomPlacementConstraints:(NSArray *)value { value=[value copy]; [_customPlacementConstraints release]; _customPlacementConstraints=value; [_gridView _relayout]; }

-initWithCoder:(NSCoder *)coder { return [self init]; }
-(void)encodeWithCoder:(NSCoder *)coder { }

@end

@implementation NSGridView

-initWithFrame:(NSRect)frame {
   self=[super initWithFrame:frame];
   if(self!=nil){
    _rows=[[NSMutableArray alloc] init];
    _columns=[[NSMutableArray alloc] init];
    _cells=[[NSMutableDictionary alloc] init];
    _xPlacement=NSGridCellPlacementLeading;
    _yPlacement=NSGridCellPlacementBottom;
    _rowSpacing=6;
    _columnSpacing=6;
    _gridConstraints=[[NSMutableArray alloc] init];
    _columnGuides=[[NSMutableArray alloc] init];
    _rowGuides=[[NSMutableArray alloc] init];
    _spanGuides=[[NSMutableArray alloc] init];
   }
   return self;
}

-(void)dealloc {
   [_rows release];
   [_columns release];
   [_cells release];
   [_gridConstraints release];
   [_columnGuides release];
   [_rowGuides release];
   [_spanGuides release];
   [super dealloc];
}

-(BOOL)isFlipped {
   return YES;
}

+(instancetype)gridViewWithNumberOfColumns:(NSInteger)columnCount rows:(NSInteger)rowCount {
   NSGridView *grid=[[[self alloc] initWithFrame:NSZeroRect] autorelease];
   NSInteger   i;

   for(i=0;i<columnCount;i++)
    [grid addColumnWithViews:[NSArray array]];
   for(i=0;i<rowCount;i++)
    [grid addRowWithViews:[NSArray array]];
   return grid;
}

+(instancetype)gridViewWithViews:(NSArray *)rows {
   NSGridView *grid=[[[self alloc] initWithFrame:NSZeroRect] autorelease];

   for(NSArray *row in rows)
    [grid addRowWithViews:row];
   [grid setFrameSize:[grid fittingSize]];
   return grid;
}

static NSString *cellKey(NSInteger column,NSInteger row){
   return [NSString stringWithFormat:@"%ld,%ld",(long)column,(long)row];
}

-(NSGridCell *)_cellAtColumn:(NSInteger)column row:(NSInteger)row {
   NSGridCell *cell=[_cells objectForKey:cellKey(column,row)];

   if(cell==nil && column>=0 && row>=0 && column<(NSInteger)[_columns count] && row<(NSInteger)[_rows count]){
    cell=[[[NSGridCell alloc] init] autorelease];
    [cell _setGridView:self row:[_rows objectAtIndex:row] column:[_columns objectAtIndex:column]];
    [_cells setObject:cell forKey:cellKey(column,row)];
   }
   return cell;
}

-(NSInteger)numberOfRows { return [_rows count]; }
-(NSInteger)numberOfColumns { return [_columns count]; }
-(NSGridRow *)rowAtIndex:(NSInteger)index { return [_rows objectAtIndex:index]; }
-(NSInteger)indexOfRow:(NSGridRow *)row { NSUInteger i=[_rows indexOfObjectIdenticalTo:row]; return (i==NSNotFound)?-1:(NSInteger)i; }
-(NSGridColumn *)columnAtIndex:(NSInteger)index { return [_columns objectAtIndex:index]; }
-(NSInteger)indexOfColumn:(NSGridColumn *)column { NSUInteger i=[_columns indexOfObjectIdenticalTo:column]; return (i==NSNotFound)?-1:(NSInteger)i; }

-(NSGridCell *)cellAtColumnIndex:(NSInteger)columnIndex rowIndex:(NSInteger)rowIndex {
   if(columnIndex<0 || rowIndex<0 || columnIndex>=(NSInteger)[_columns count] || rowIndex>=(NSInteger)[_rows count])
    [NSException raise:NSRangeException format:@"-[NSGridView %s] cell %ld,%ld is outside the grid",sel_getName(_cmd),(long)columnIndex,(long)rowIndex];
   return [self _cellAtColumn:columnIndex row:rowIndex];
}

-(NSGridCell *)cellForView:(NSView *)view {
   for(NSGridCell *cell in [_cells allValues])
    if([cell contentView]==view)
     return cell;
   return nil;
}

/* The cells are re-keyed after rows or columns move. */
-(void)_rekeyCells {
   NSMutableDictionary *cells=[NSMutableDictionary dictionary];

   for(NSGridCell *cell in [_cells allValues]){
    NSInteger row=[self indexOfRow:[cell row]],column=[self indexOfColumn:[cell column]];

    if(row>=0 && column>=0)
     [cells setObject:cell forKey:cellKey(column,row)];
    else if(isContent([cell contentView]))
     [[cell contentView] removeFromSuperview];
   }
   [_cells setDictionary:cells];
}

-(NSGridRow *)insertRowAtIndex:(NSInteger)index withViews:(NSArray *)views {
   NSGridRow *row=[[[NSGridRow alloc] init] autorelease];
   NSInteger  i;

   [row _setGridView:self];
   [row setHeight:NSGridViewSizeForContent];
   [_rows insertObject:row atIndex:index];
   while((NSInteger)[_columns count]<(NSInteger)[views count]){
    NSGridColumn *column=[[[NSGridColumn alloc] init] autorelease];

    [column _setGridView:self];
    [column setWidth:NSGridViewSizeForContent];
    [_columns addObject:column];
   }
   [self _rekeyCells];
   for(i=0;i<(NSInteger)[views count];i++)
    [[self _cellAtColumn:i row:index] setContentView:[views objectAtIndex:i]];
   [self _relayout];
   return row;
}

-(NSGridRow *)addRowWithViews:(NSArray *)views {
   return [self insertRowAtIndex:[_rows count] withViews:views];
}

-(void)moveRowAtIndex:(NSInteger)fromIndex toIndex:(NSInteger)toIndex {
   NSGridRow *row=[[[_rows objectAtIndex:fromIndex] retain] autorelease];

   [_rows removeObjectAtIndex:fromIndex];
   [_rows insertObject:row atIndex:toIndex];
   [self _rekeyCells];
   [self _relayout];
}

-(void)removeRowAtIndex:(NSInteger)index {
   [_rows removeObjectAtIndex:index];
   [self _rekeyCells];
   [self _relayout];
}

-(NSGridColumn *)insertColumnAtIndex:(NSInteger)index withViews:(NSArray *)views {
   NSGridColumn *column=[[[NSGridColumn alloc] init] autorelease];
   NSInteger     i;

   [column _setGridView:self];
   [column setWidth:NSGridViewSizeForContent];
   [_columns insertObject:column atIndex:index];
   while((NSInteger)[_rows count]<(NSInteger)[views count]){
    NSGridRow *row=[[[NSGridRow alloc] init] autorelease];

    [row _setGridView:self];
    [row setHeight:NSGridViewSizeForContent];
    [_rows addObject:row];
   }
   [self _rekeyCells];
   for(i=0;i<(NSInteger)[views count];i++)
    [[self _cellAtColumn:index row:i] setContentView:[views objectAtIndex:i]];
   [self _relayout];
   return column;
}

-(NSGridColumn *)addColumnWithViews:(NSArray *)views {
   return [self insertColumnAtIndex:[_columns count] withViews:views];
}

-(void)moveColumnAtIndex:(NSInteger)fromIndex toIndex:(NSInteger)toIndex {
   NSGridColumn *column=[[[_columns objectAtIndex:fromIndex] retain] autorelease];

   [_columns removeObjectAtIndex:fromIndex];
   [_columns insertObject:column atIndex:toIndex];
   [self _rekeyCells];
   [self _relayout];
}

-(void)removeColumnAtIndex:(NSInteger)index {
   [_columns removeObjectAtIndex:index];
   [self _rekeyCells];
   [self _relayout];
}

-(NSGridCellPlacement)xPlacement { return _xPlacement; }
-(void)setXPlacement:(NSGridCellPlacement)value { _xPlacement=value; [self _relayout]; }
-(NSGridCellPlacement)yPlacement { return _yPlacement; }
-(void)setYPlacement:(NSGridCellPlacement)value { _yPlacement=value; [self _relayout]; }
-(NSGridRowAlignment)rowAlignment { return _rowAlignment; }
-(void)setRowAlignment:(NSGridRowAlignment)value { _rowAlignment=value; [self _relayout]; }
-(CGFloat)rowSpacing { return _rowSpacing; }
-(void)setRowSpacing:(CGFloat)value { _rowSpacing=value; [self _relayout]; }
-(CGFloat)columnSpacing { return _columnSpacing; }
-(void)setColumnSpacing:(CGFloat)value { _columnSpacing=value; [self _relayout]; }

-(void)mergeCellsInHorizontalRange:(NSRange)hRange verticalRange:(NSRange)vRange {
   NSGridCell *head=[self cellAtColumnIndex:hRange.location rowIndex:vRange.location];
   NSUInteger  column,row;

   for(row=vRange.location;row<NSMaxRange(vRange);row++)
    for(column=hRange.location;column<NSMaxRange(hRange);column++){
     NSGridCell *cell=[self cellAtColumnIndex:column rowIndex:row];

     if(cell!=head){
      [cell _setHead:head];
      if(isContent([cell contentView]))
       [[cell contentView] removeFromSuperview];
     }
    }
   [self _relayout];
}

/* Minkar: the grid is laid out by constraints, as on the reference
   platform: each column and row is a layout guide, one after the other
   with the paddings and spacing between them; a view is held inside its
   cell (or the area of its merged cells) and placed in it, and the views
   of a row aligned on their baselines when the row asks for it. Columns
   and rows are as small as their views allow (or as set); the room the
   grid has beyond them goes to the columns of views with no width of
   their own, else stays past the last column and row. Placed by frames,
   the grid fought the constraints apps add between its views (TextMate's
   equal widths), and its window was laid out without end. */

/* Columns and rows hug their views a little more than the grid hugs them,
   so that room the grid is given goes past them; never so much that, with
   what else pulls on a view (a fitting size), they overcome its
   compression resistance: the solver adds priorities up. */
static NSLayoutPriority lineHugging(NSLayoutPriority gridHugging){
   return MAX(249,MIN(gridHugging+1,699));
}
static const NSLayoutPriority NSGridStretching=1;
// Inside its cell, unless a set column or row is too small, and placed in
// it; a grid whose frame is set keeps it.
static const NSLayoutPriority NSGridContainment=999;
static const NSLayoutPriority NSGridPlacement=998;

-(void)_relayout {
   [self setNeedsUpdateConstraints:YES];
}

-(void)_add:(NSLayoutConstraint *)constraint priority:(NSLayoutPriority)priority {
   [constraint setPriority:priority];
   [_gridConstraints addObject:constraint];
}

static NSLayoutConstraint *relate(id item,NSLayoutAttribute attribute,NSLayoutRelation relation,id other,NSLayoutAttribute otherAttribute,CGFloat constant){
   return [NSLayoutConstraint constraintWithItem:item attribute:attribute relatedBy:relation toItem:other attribute:otherAttribute multiplier:1 constant:constant];
}

/* One guide per column or row, kept between updates. */
-(void)_guides:(NSMutableArray *)guides count:(NSUInteger)count {
   while([guides count]<count){
    NSLayoutGuide *guide=[[NSLayoutGuide alloc] init];

    [self addLayoutGuide:guide];
    [guides addObject:guide];
    [guide release];
   }
   while([guides count]>count){
    [self removeLayoutGuide:[guides lastObject]];
    [guides removeLastObject];
   }
}

/* Columns (horizontal) or rows (vertical): each after the previous shown
   one, its width or height set or hugging its views, and the grid's edge
   past the last. */
-(void)_chain:(NSArray *)lines guides:(NSArray *)guides horizontal:(BOOL)horizontal stretching:(BOOL *)stretching {
   NSLayoutAttribute start=horizontal?NSLayoutAttributeLeading:NSLayoutAttributeTop;
   NSLayoutAttribute end=horizontal?NSLayoutAttributeTrailing:NSLayoutAttributeBottom;
   NSLayoutAttribute size=horizontal?NSLayoutAttributeWidth:NSLayoutAttributeHeight;
   CGFloat           spacing=horizontal?_columnSpacing:_rowSpacing;
   id                previous=self;
   NSLayoutAttribute previousAttribute=start;
   CGFloat           previousConstant=0;
   BOOL              shown=NO;
   NSUInteger        i;
   NSLayoutPriority  hugging=lineHugging([self contentHuggingPriorityForOrientation:horizontal?NSLayoutConstraintOrientationHorizontal:NSLayoutConstraintOrientationVertical]);

   for(i=0;i<[lines count];i++){
    id            line=[lines objectAtIndex:i];
    NSLayoutGuide *guide=[guides objectAtIndex:i];
    CGFloat       set=horizontal?[(NSGridColumn *)line width]:[(NSGridRow *)line height];
    CGFloat       before=horizontal?[(NSGridColumn *)line leadingPadding]:[(NSGridRow *)line topPadding];
    CGFloat       after=horizontal?[(NSGridColumn *)line trailingPadding]:[(NSGridRow *)line bottomPadding];

    if([line isHidden]){
     [self _add:relate(guide,start,NSLayoutRelationEqual,previous,previousAttribute,previousConstant) priority:NSLayoutPriorityRequired];
     [self _add:relate(guide,size,NSLayoutRelationEqual,nil,NSLayoutAttributeNotAnAttribute,0) priority:NSLayoutPriorityRequired];
     continue;
    }
    [self _add:relate(guide,start,NSLayoutRelationEqual,previous,previousAttribute,previousConstant+before) priority:NSLayoutPriorityRequired];
    if(set!=NSGridViewSizeForContent && set>0)
     [self _add:relate(guide,size,NSLayoutRelationEqual,nil,NSLayoutAttributeNotAnAttribute,set) priority:NSLayoutPriorityRequired];
    else {
     [self _add:relate(guide,size,NSLayoutRelationGreaterThanOrEqual,nil,NSLayoutAttributeNotAnAttribute,0) priority:NSLayoutPriorityRequired];
     [self _add:relate(guide,size,NSLayoutRelationEqual,nil,NSLayoutAttributeNotAnAttribute,0) priority:(stretching!=NULL && stretching[i])?NSGridStretching:hugging];
    }
    previous=guide;
    previousAttribute=end;
    previousConstant=after+spacing;
    shown=YES;
   }
   if(shown){
    NSLayoutConstraintOrientation orientation=horizontal?NSLayoutConstraintOrientationHorizontal:NSLayoutConstraintOrientationVertical;

    previousConstant-=spacing;
    [self _add:relate(self,end,NSLayoutRelationGreaterThanOrEqual,previous,previousAttribute,previousConstant) priority:NSGridContainment];
    [self _add:relate(self,end,NSLayoutRelationEqual,previous,previousAttribute,previousConstant) priority:MIN([self contentHuggingPriorityForOrientation:orientation],hugging-1)];
   }
}

/* A view placed between two edges along one axis: at one of them, the
   other, centred or filling, and kept between them. */
-(void)_place:(NSView *)view placement:(NSGridCellPlacement)placement from:(id)first to:(id)last area:(id)area horizontal:(BOOL)horizontal {
   NSLayoutAttribute start=horizontal?NSLayoutAttributeLeading:NSLayoutAttributeTop;
   NSLayoutAttribute end=horizontal?NSLayoutAttributeTrailing:NSLayoutAttributeBottom;
   NSLayoutAttribute center=horizontal?NSLayoutAttributeCenterX:NSLayoutAttributeCenterY;

   if(placement==NSGridCellPlacementNone)
    return;
   [self _add:relate(view,start,NSLayoutRelationGreaterThanOrEqual,first,start,0) priority:NSGridContainment];
   [self _add:relate(view,end,NSLayoutRelationLessThanOrEqual,last,end,0) priority:NSGridContainment];
   switch(placement){
    case NSGridCellPlacementTrailing:
     [self _add:relate(view,end,NSLayoutRelationEqual,last,end,0) priority:NSGridPlacement];
     break;
    case NSGridCellPlacementCenter:
     [self _add:relate(view,center,NSLayoutRelationEqual,area,center,0) priority:NSGridPlacement];
     break;
    case NSGridCellPlacementFill:
     [self _add:relate(view,start,NSLayoutRelationEqual,first,start,0) priority:NSGridPlacement];
     [self _add:relate(view,end,NSLayoutRelationEqual,last,end,0) priority:NSGridPlacement];
     break;
    default:
     [self _add:relate(view,start,NSLayoutRelationEqual,first,start,0) priority:NSGridPlacement];
     break;
   }
}

-(void)updateConstraints {
   NSInteger columns=[_columns count],rows=[_rows count],column,row;
   BOOL      stretching[columns>0?columns:1];
   NSView   *baselineView[rows>0?rows:1];

   [self removeConstraints:_gridConstraints];
   [_gridConstraints removeAllObjects];
   for(NSLayoutGuide *guide in _spanGuides)
    [self removeLayoutGuide:guide];
   [_spanGuides removeAllObjects];
   [self _guides:_columnGuides count:columns];
   [self _guides:_rowGuides count:rows];

   for(column=0;column<columns;column++){
    stretching[column]=NO;
    for(row=0;row<rows;row++){
     NSGridCell *cell=[_cells objectForKey:cellKey(column,row)];

     if(cell!=nil && [cell _head]==nil && isContent([cell contentView]) && stretchesAcross([cell contentView]))
      stretching[column]=YES;
    }
   }
   [self _chain:_columns guides:_columnGuides horizontal:YES stretching:stretching];
   [self _chain:_rows guides:_rowGuides horizontal:NO stretching:NULL];

   for(row=0;row<rows;row++)
    baselineView[row]=nil;
   for(row=0;row<rows;row++)
    for(column=0;column<columns;column++){
     NSGridCell *cell=[_cells objectForKey:cellKey(column,row)];
     NSView     *view=[cell contentView];
     NSInteger   lastColumn=column,lastRow=row;
     NSGridCellPlacement xPlacement,yPlacement;
     NSGridRowAlignment  alignment;
     id          firstColumnGuide,lastColumnGuide,firstRowGuide,lastRowGuide,area;

     if(!isContent(view) || [cell _head]!=nil)
      continue;
     if([[_columns objectAtIndex:column] isHidden] || [[_rows objectAtIndex:row] isHidden]){
      [view setHidden:YES];
      continue;
     }
     if([view superview]!=self)
      [self addSubview:view];
     [view setHidden:NO];
     /* A merged cell reaches over the cells merged into it. */
     while(lastColumn+1<columns && [[_cells objectForKey:cellKey(lastColumn+1,row)] _head]==cell)
      lastColumn++;
     while(lastRow+1<rows && [[_cells objectForKey:cellKey(column,lastRow+1)] _head]==cell)
      lastRow++;
     firstColumnGuide=[_columnGuides objectAtIndex:column];
     lastColumnGuide=[_columnGuides objectAtIndex:lastColumn];
     firstRowGuide=[_rowGuides objectAtIndex:row];
     lastRowGuide=[_rowGuides objectAtIndex:lastRow];
     // Centred in merged cells: in a guide over their area.
     area=nil;
     if(lastColumn!=column || lastRow!=row){
      NSLayoutGuide *span=[[[NSLayoutGuide alloc] init] autorelease];

      [self addLayoutGuide:span];
      [_spanGuides addObject:span];
      [self _add:relate(span,NSLayoutAttributeLeading,NSLayoutRelationEqual,firstColumnGuide,NSLayoutAttributeLeading,0) priority:NSLayoutPriorityRequired];
      [self _add:relate(span,NSLayoutAttributeTrailing,NSLayoutRelationEqual,lastColumnGuide,NSLayoutAttributeTrailing,0) priority:NSLayoutPriorityRequired];
      [self _add:relate(span,NSLayoutAttributeTop,NSLayoutRelationEqual,firstRowGuide,NSLayoutAttributeTop,0) priority:NSLayoutPriorityRequired];
      [self _add:relate(span,NSLayoutAttributeBottom,NSLayoutRelationEqual,lastRowGuide,NSLayoutAttributeBottom,0) priority:NSLayoutPriorityRequired];
      area=span;
     }

     xPlacement=[cell xPlacement];
     if(xPlacement==NSGridCellPlacementInherited)
      xPlacement=[[cell column] xPlacement];
     if(xPlacement==NSGridCellPlacementInherited)
      xPlacement=_xPlacement;
     if(xPlacement==NSGridCellPlacementInherited)
      xPlacement=NSGridCellPlacementLeading;
     if(stretchesAcross(view) && xPlacement!=NSGridCellPlacementNone)
      xPlacement=NSGridCellPlacementFill;
     yPlacement=[cell yPlacement];
     if(yPlacement==NSGridCellPlacementInherited)
      yPlacement=[[cell row] yPlacement];
     if(yPlacement==NSGridCellPlacementInherited)
      yPlacement=_yPlacement;
     if(yPlacement==NSGridCellPlacementInherited)
      yPlacement=NSGridCellPlacementBottom;
     alignment=[cell rowAlignment];
     if(alignment==NSGridRowAlignmentInherited)
      alignment=[[cell row] rowAlignment];
     if(alignment==NSGridRowAlignmentInherited)
      alignment=_rowAlignment;

     [self _place:view placement:xPlacement from:firstColumnGuide to:lastColumnGuide area:(area!=nil?area:firstColumnGuide) horizontal:YES];
     /* On baselines: the views of the row share them, and their placement
        only says where the aligned views go in a taller row. */
     if((alignment==NSGridRowAlignmentFirstBaseline || alignment==NSGridRowAlignmentLastBaseline) && lastRow==row){
      NSLayoutAttribute baseline=(alignment==NSGridRowAlignmentFirstBaseline)?NSLayoutAttributeFirstBaseline:NSLayoutAttributeLastBaseline;

      if(baselineView[row]==nil)
       baselineView[row]=view;
      else
       [self _add:relate(view,baseline,NSLayoutRelationEqual,baselineView[row],baseline,0) priority:NSLayoutPriorityRequired];
      if(yPlacement!=NSGridCellPlacementNone){
       [self _add:relate(view,NSLayoutAttributeTop,NSLayoutRelationGreaterThanOrEqual,firstRowGuide,NSLayoutAttributeTop,0) priority:NSGridContainment];
       [self _add:relate(view,NSLayoutAttributeBottom,NSLayoutRelationLessThanOrEqual,lastRowGuide,NSLayoutAttributeBottom,0) priority:NSGridContainment];
      }
     }
     else
      [self _place:view placement:yPlacement from:firstRowGuide to:lastRowGuide area:(area!=nil?area:firstRowGuide) horizontal:NO];
     for(NSLayoutConstraint *constraint in [cell customPlacementConstraints])
      [self _add:constraint priority:[constraint priority]];
    }

   [self addConstraints:_gridConstraints];
   [super updateConstraints];
}

@end
