/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSGridView.h>
#import <AppKit/NSLayoutConstraint.h>
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

/* A view's size in a cell: its intrinsic size where it has one, else its
   frame's. */
static NSSize contentSizeOfView(NSView *view){
   NSSize size=[view frame].size;
   NSSize intrinsic;

   if(view==nil)
    return NSZeroSize;
   intrinsic=[view intrinsicContentSize];
   if(intrinsic.width!=NSViewNoIntrinsicMetric)
    size.width=intrinsic.width;
   if(intrinsic.height!=NSViewNoIntrinsicMetric)
    size.height=intrinsic.height;
   return size;
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
   [_contentView removeFromSuperview];
   view=[view retain];
   [_contentView release];
   _contentView=view;
   if(view!=nil && _gridView!=nil)
    [_gridView addSubview:view];
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
-(void)setCustomPlacementConstraints:(NSArray *)value { value=[value copy]; [_customPlacementConstraints release]; _customPlacementConstraints=value; }

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
   }
   return self;
}

-(void)dealloc {
   [_rows release];
   [_columns release];
   [_cells release];
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
    else
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
      [[cell contentView] removeFromSuperview];
     }
    }
   [self _relayout];
}

/* Column widths and row heights: set ones, or the largest content of
   their unmerged cells; then each view placed in its cell (or in its
   merged cells' area). */
-(void)_computeWidths:(CGFloat *)widths heights:(CGFloat *)heights {
   NSInteger column,row,columns=[_columns count],rows=[_rows count];

   for(column=0;column<columns;column++){
    NSGridColumn *c=[_columns objectAtIndex:column];

    widths[column]=0;
    if([c isHidden])
     continue;
    if([c width]!=NSGridViewSizeForContent && [c width]>0)
     widths[column]=[c width];
    else
     for(row=0;row<rows;row++){
      NSGridCell *cell=[_cells objectForKey:cellKey(column,row)];

      if(cell!=nil && [cell _head]==nil && ![[_rows objectAtIndex:row] isHidden])
       widths[column]=MAX(widths[column],contentSizeOfView([cell contentView]).width);
     }
   }
   for(row=0;row<rows;row++){
    NSGridRow *r=[_rows objectAtIndex:row];

    heights[row]=0;
    if([r isHidden])
     continue;
    if([r height]!=NSGridViewSizeForContent && [r height]>0)
     heights[row]=[r height];
    else
     for(column=0;column<columns;column++){
      NSGridCell *cell=[_cells objectForKey:cellKey(column,row)];

      if(cell!=nil && [cell _head]==nil && ![[_columns objectAtIndex:column] isHidden])
       heights[row]=MAX(heights[row],contentSizeOfView([cell contentView]).height);
     }
   }
}

-(NSSize)fittingSize {
   NSInteger columns=[_columns count],rows=[_rows count],i,shownColumns=0,shownRows=0;
   CGFloat   widths[columns>0?columns:1],heights[rows>0?rows:1],width=0,height=0;

   [self _computeWidths:widths heights:heights];
   for(i=0;i<columns;i++){
    NSGridColumn *c=[_columns objectAtIndex:i];

    if([c isHidden])
     continue;
    width+=widths[i]+[c leadingPadding]+[c trailingPadding];
    shownColumns++;
   }
   for(i=0;i<rows;i++){
    NSGridRow *r=[_rows objectAtIndex:i];

    if([r isHidden])
     continue;
    height+=heights[i]+[r topPadding]+[r bottomPadding];
    shownRows++;
   }
   width+=_columnSpacing*MAX(0,shownColumns-1);
   height+=_rowSpacing*MAX(0,shownRows-1);
   return NSMakeSize(width,height);
}

-(NSSize)intrinsicContentSize {
   return [self fittingSize];
}

static CGFloat placed(CGFloat origin,CGFloat available,CGFloat size,NSGridCellPlacement placement,CGFloat *outSize){
   switch(placement){
    case NSGridCellPlacementFill:
     *outSize=available;
     return origin;
    case NSGridCellPlacementTrailing:
     *outSize=size;
     return origin+available-size;
    case NSGridCellPlacementCenter:
     *outSize=size;
     return origin+floor((available-size)/2);
    default:
     *outSize=size;
     return origin;
   }
}

-(void)_relayout {
   NSInteger columns=[_columns count],rows=[_rows count],column,row;
   CGFloat   widths[columns>0?columns:1],heights[rows>0?rows:1],xs[columns>0?columns:1],ys[rows>0?rows:1];
   CGFloat   x=0,y=0;

   if(_layingOut)
    return;
   _layingOut=YES;
   [self _computeWidths:widths heights:heights];
   for(column=0;column<columns;column++){
    NSGridColumn *c=[_columns objectAtIndex:column];

    x+=[c leadingPadding];
    xs[column]=x;
    if(![c isHidden])
     x+=widths[column]+[c trailingPadding]+_columnSpacing;
   }
   for(row=0;row<rows;row++){
    NSGridRow *r=[_rows objectAtIndex:row];

    y+=[r topPadding];
    ys[row]=y;
    if(![r isHidden])
     y+=heights[row]+[r bottomPadding]+_rowSpacing;
   }
   for(NSString *key in [_cells allKeys]){
    NSGridCell *cell=[_cells objectForKey:key];
    NSView     *view=[cell contentView];
    NSInteger   c=[self indexOfColumn:[cell column]],r=[self indexOfRow:[cell row]];
    NSInteger   lastColumn=c,lastRow=r;
    NSGridCellPlacement xPlacement,yPlacement;
    NSSize      size;
    CGFloat     width,height,w,h;

    if(view==nil || [cell _head]!=nil || c<0 || r<0)
     continue;
    if([[_columns objectAtIndex:c] isHidden] || [[_rows objectAtIndex:r] isHidden]){
     [view setHidden:YES];
     continue;
    }
    if([view superview]!=self)
     [self addSubview:view];
    /* A merged cell reaches over the cells merged into it. */
    while(lastColumn+1<columns && [[_cells objectForKey:cellKey(lastColumn+1,r)] _head]==cell)
     lastColumn++;
    while(lastRow+1<rows && [[_cells objectForKey:cellKey(c,lastRow+1)] _head]==cell)
     lastRow++;
    width=xs[lastColumn]+widths[lastColumn]-xs[c];
    height=ys[lastRow]+heights[lastRow]-ys[r];
    xPlacement=[cell xPlacement];
    if(xPlacement==NSGridCellPlacementInherited)
     xPlacement=[[cell column] xPlacement];
    if(xPlacement==NSGridCellPlacementInherited)
     xPlacement=_xPlacement;
    yPlacement=[cell yPlacement];
    if(yPlacement==NSGridCellPlacementInherited)
     yPlacement=[[cell row] yPlacement];
    if(yPlacement==NSGridCellPlacementInherited)
     yPlacement=_yPlacement;
    size=contentSizeOfView(view);
    CGFloat vx=placed(xs[c],width,MIN(size.width,width),xPlacement,&w);
    CGFloat vy=placed(ys[r],height,MIN(size.height,height),yPlacement,&h);
    [view setHidden:NO];
    [view setFrame:NSMakeRect(vx,vy,w,h)];
   }
   _layingOut=NO;
   [self invalidateIntrinsicContentSize];
}

-(void)resizeSubviewsWithOldSize:(NSSize)oldSize {
   [self _relayout];
}

-(void)layout {
   [super layout];
   [self _relayout];
}

@end
