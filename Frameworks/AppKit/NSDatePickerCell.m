/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <sys/param.h>
#import <AppKit/NSDatePickerCell.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSStringDrawing.h>
#import <AppKit/NSParagraphStyle.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSGraphicsStyle.h>
#import <AppKit/NSFont.h>
#import <Foundation/NSDateFormatter.h>
#import <Foundation/NSCalendar.h>
#import <Foundation/NSLocale.h>

@implementation NSDatePickerCell

/* A cell made in code (NSDatePicker's own, for one): today's date, shown as
   a text field with a stepper, in the system's font and colors. */
-initTextCell:(NSString *)string {
   [super initTextCell:string];
   _elements=NSYearMonthDayDatePickerElementFlag;
   _style=NSTextFieldAndStepperDatePickerStyle;
   _calendar=[[NSCalendar currentCalendar] copy];
   _backgroundColor=[[NSColor controlBackgroundColor] copy];
   _textColor=[[NSColor controlTextColor] copy];
   _drawsBackground=YES;
   [self setObjectValue:[NSDate date]];
   [self setBezeled:YES];
   return self;
}

-init {
   return [self initTextCell:@""];
}

-copyWithZone:(NSZone *)zone {
   NSDatePickerCell *copy=[super copyWithZone:zone];

   copy->_calendar=[_calendar copy];
   copy->_locale=[_locale copy];
   copy->_minDate=[_minDate copy];
   copy->_maxDate=[_maxDate copy];
   copy->_timeZone=[_timeZone copy];
   copy->_backgroundColor=[_backgroundColor copy];
   copy->_textColor=[_textColor copy];
   return copy;
}

-(void)dealloc {
   [_calendar release];
   [_locale release];
   [_minDate release];
   [_maxDate release];
   [_timeZone release];
   [_backgroundColor release];
   [_textColor release];
   [super dealloc];
}

-initWithCoder:(NSCoder *)coder {
   [super initWithCoder:coder];

   if(![coder allowsKeyedCoding]){
    [NSException raise:NSInvalidArgumentException format:@"%@ can not initWithCoder:%@",[self class],[coder class]];
    return self;
   }

   _elements=[coder decodeIntegerForKey:@"NSDatePickerElements"];
   _minDate=[[coder decodeObjectForKey:@"NSMinDate"] copy];
   _backgroundColor=[[coder decodeObjectForKey:@"NSBackgroundColor"] copy];
   _textColor=[[coder decodeObjectForKey:@"NSTextColor"] copy];
   _timeInterval=[coder decodeDoubleForKey:@"NSTimeInterval"];
   _calendar=[[NSCalendar currentCalendar] copy];
   // NSDatePicker has an NSEnabled field,
   [self setEnabled:YES];

   return self;
}

-delegate {
   return _delegate;
}

-(NSDatePickerElementFlags)datePickerElements {
   return _elements;
}

-(NSDatePickerMode)datePickerMode {
   return _mode;
}

-(NSDatePickerStyle)datePickerStyle {
   return _style;
}

-(NSCalendar *)calendar {
   return _calendar;
}

-(NSLocale *)locale {
   return _locale;
}

-(NSDate *)minDate {
   return _minDate;
}

-(NSDate *)maxDate {
   return _maxDate;
}

-(NSDate *)dateValue {
   return [self objectValue];
}

-(NSTimeInterval)timeInterval {
   return 0;
}

-(NSTimeZone *)timeZone {
   return _timeZone;
}

-(BOOL)drawsBackground {
   return _drawsBackground;
}

-(NSColor *)backgroundColor {
   return _backgroundColor;
}

-(NSColor *)textColor {
   return _textColor;
}

-(void)setDelegate:delegate {
   _delegate=delegate;
}

-(void)setDatePickerElements:(NSDatePickerElementFlags)elements {
   _elements=elements;
}

-(void)setDatePickerMode:(NSDatePickerMode)mode {
   _mode=mode;
}

-(void)setDatePickerStyle:(NSDatePickerStyle)style {
   _style=style;
}

-(void)setCalendar:(NSCalendar *)calendar {
   calendar=[calendar copy];
   [_calendar release];
   _calendar=calendar;
}

-(void)setLocale:(NSLocale *)locale {
   locale=[locale copy];
   [_locale release];
   _locale=locale;
}

-(void)setMinDate:(NSDate *)date {
   date=[date copy];
   [_minDate release];
   _minDate=date;
}

-(void)setMaxDate:(NSDate *)date {
   date=[date copy];
   [_maxDate release];
   _maxDate=date;
}

-(void)setDateValue:(NSDate *)date {
   [self setObjectValue:date];
}

-(void)setTimeInterval:(NSTimeInterval)interval {
}

-(void)setTimeZone:(NSTimeZone *)timeZone {
   timeZone=[timeZone copy];
   [_timeZone release];
   _timeZone=timeZone;
}

-(void)setDrawsBackground:(BOOL)flag {
   _drawsBackground=flag;
}

-(void)setBackgroundColor:(NSColor *)color {
   color=[color copy];
   [_backgroundColor release];
   _backgroundColor=color;
}

-(void)setTextColor:(NSColor *)color {
   color=[color copy];
   [_textColor release];
   _textColor=color;
}

static NSSize maxDigitSize(NSDictionary *attributes){
   NSString *digits[10]={
    @"0",@"1",@"2",@"3",@"4",@"5",@"6",@"7",@"8",@"9"
   };
   NSSize result={0};
   NSInteger i;

   for(i=0;i<10;i++){
    NSSize check=[digits[i] sizeWithAttributes:attributes];

    result.width=MAX(result.width,check.width);
    result.height=MAX(result.height,check.height);
   }

   return result;
}

static void drawRoundedSelection(NSRect rect){
   [[NSColor selectedTextBackgroundColor] set];
   NSBezierPath *path=[NSBezierPath bezierPathWithRoundedRect:rect xRadius:3 yRadius:3];
   [path fill];
}

-(NSArray *)_attributedStrings {
   NSMutableArray          *result=[NSMutableArray array];
   NSMutableParagraphStyle *style=[[[NSParagraphStyle defaultParagraphStyle] mutableCopy] autorelease];
   NSMutableDictionary     *attributes=[NSMutableDictionary dictionary];
   NSMutableDictionary     *separatorAttributes;

   [style setAlignment:NSRightTextAlignment];
   [attributes setObject:style forKey:NSParagraphStyleAttributeName];
   if([self font]!=nil)
    [attributes setObject:[self font] forKey:NSFontAttributeName];
   [attributes setObject:[self textColor] forKey:NSForegroundColorAttributeName];

   NSSize digitSize=maxDigitSize(attributes);
   CGFloat digitWidth=digitSize.width;

   separatorAttributes=[[attributes mutableCopy] autorelease];

   if((_elements&NSYearMonthDayDatePickerElementFlag)==NSYearMonthDayDatePickerElementFlag){
    NSUInteger        flags=NSYearCalendarUnit|NSMonthCalendarUnit|NSDayCalendarUnit;
    NSDateComponents *components=[_calendar components:flags fromDate:[self dateValue]];

    NSString *month=[NSString stringWithFormat:@"%d",[components month]];
    NSString *day=[NSString stringWithFormat:@"%d",[components day]];
    NSString *year=[NSString stringWithFormat:@"%d",[components year]];

    [attributes setObject:[NSNumber numberWithUnsignedInteger:[components month]] forKey:@"_value"];
    [attributes setObject:[NSNumber numberWithUnsignedInteger:1] forKey:@"_min"];
    [attributes setObject:[NSNumber numberWithUnsignedInteger:12] forKey:@"_max"];
    [attributes setObject:[NSNumber numberWithUnsignedInteger:NSMonthCalendarUnit] forKey:@"_unit"];
    [attributes setObject:[NSNumber numberWithFloat:digitWidth*2] forKey:@"_width"];
    [result addObject:[[[NSAttributedString alloc] initWithString:month attributes:attributes] autorelease]];

    [result addObject:[[[NSAttributedString alloc] initWithString:@"/" attributes:separatorAttributes] autorelease]];

    [attributes setObject:[NSNumber numberWithUnsignedInteger:[components day]] forKey:@"_value"];
    [attributes setObject:[NSNumber numberWithUnsignedInteger:1] forKey:@"_min"];
    [attributes setObject:[NSNumber numberWithUnsignedInteger:31] forKey:@"_max"];
    [attributes setObject:[NSNumber numberWithUnsignedInteger:NSDayCalendarUnit] forKey:@"_unit"];
    [attributes setObject:[NSNumber numberWithFloat:digitWidth*2] forKey:@"_width"];
    [result addObject:[[[NSAttributedString alloc] initWithString:day attributes:attributes] autorelease]];

    [result addObject:[[[NSAttributedString alloc] initWithString:@"/" attributes:separatorAttributes] autorelease]];

    [attributes setObject:[NSNumber numberWithUnsignedInteger:[components year]] forKey:@"_value"];
    [attributes setObject:[NSNumber numberWithUnsignedInteger:0] forKey:@"_min"];
    [attributes setObject:[NSNumber numberWithUnsignedInteger:9999] forKey:@"_max"];
    [attributes setObject:[NSNumber numberWithUnsignedInteger:NSYearCalendarUnit] forKey:@"_unit"];
    [attributes setObject:[NSNumber numberWithFloat:digitWidth*4] forKey:@"_width"];
    [result addObject:[[[NSAttributedString alloc] initWithString:year attributes:attributes] autorelease]];
   }

   return result;
}

static void getRectsInFrameForAttributesStrings(NSRect *rects,NSRect frame,NSArray *array){
   NSInteger i,count=[array count];
   CGFloat   maxHeight=0;

   for(i=0;i<count;i++){
    NSAttributedString *check=[array objectAtIndex:i];
    NSSize              size=[check size];
    NSDictionary       *attributes=[check attributesAtIndex:0 effectiveRange:NULL];
    NSNumber           *width=[attributes objectForKey:@"_width"];

    if(width==nil)
     rects[i].size=size;
    else {
     rects[i].size.width=[width floatValue];
     rects[i].size.height=size.height;
    }
    maxHeight=MAX(maxHeight,rects[i].size.height);
   }

   NSPoint origin=frame.origin;

   origin.y+=floor((frame.size.height-maxHeight)/2);

   for(i=0;i<count;i++){
    rects[i].origin=origin;
    origin.x+=rects[i].size.width;
   }
}

-(NSRect)_stepperFrameForFrame:(NSRect)frame {
   NSRect stepper=frame;
   stepper.origin.x=NSMaxX(stepper)-15;
   stepper.size.width=15;
   stepper=NSInsetRect(stepper,0,3);
   return stepper;
}

-(NSRect)_bezelFrameForFrame:(NSRect)frame {
   NSRect stepper=[self _stepperFrameForFrame:frame];
   NSRect bezel=frame;

   if(_style!=NSTextFieldDatePickerStyle)
    bezel.size.width-=stepper.size.width+2;
   bezel=NSInsetRect(bezel,1,3);
   return bezel;
}

-(void)drawInteriorWithFrame:(NSRect)frame inView:(NSView *)view {
   NSArray  *array=[self _attributedStrings];
   NSInteger i,count=[array count];
   NSRect    rects[count];

   frame=[self _bezelFrameForFrame:[view bounds]];

   getRectsInFrameForAttributesStrings(rects,NSInsetRect(frame,2,0),array);

   for(i=0;i<count;i++){
    NSAttributedString *string=[array objectAtIndex:i];
    NSDictionary       *attributes=[string attributesAtIndex:0 effectiveRange:NULL];
    NSNumber           *unit=[attributes objectForKey:@"_unit"];

    if([unit unsignedIntegerValue]&_selectedUnit){
     drawRoundedSelection(rects[i]);
    }

    [string drawInRect:rects[i]];
   }
}

-(void)_multiplyCurrentUnitBy:(NSInteger)multiply add:(NSInteger)addValue {
   NSArray          *array=[self _attributedStrings];
   NSInteger         i,count=[array count];
   NSDateComponents *components=[[[NSDateComponents alloc] init] autorelease];

   for(i=0;i<count;i++){
    NSAttributedString *string=[array objectAtIndex:i];
    NSDictionary       *attributes=[string attributesAtIndex:0 effectiveRange:NULL];
    NSUInteger          unit=[[attributes objectForKey:@"_unit"] unsignedIntegerValue];

    if(unit==0)
     continue;

    NSUInteger          value=[[attributes objectForKey:@"_value"] unsignedIntegerValue];
    NSUInteger          min=[[attributes objectForKey:@"_min"] unsignedIntegerValue];
    NSUInteger          max=[[attributes objectForKey:@"_max"] unsignedIntegerValue];

    if(unit==_selectedUnit){
     value*=multiply;
     value+=addValue;
     if(value>max){
      value=addValue;
     }
     if(value<min)
      value=min;
    }

    switch(unit){
     case NSYearCalendarUnit:
      [components setYear:value];
      break;

     case NSMonthCalendarUnit:
      [components setMonth:value];
      break;

     case NSDayCalendarUnit:
      [components setDay:value];
      break;
    }

   }

   NSDate *date=[_calendar dateFromComponents:components];

   [self setDateValue:date];
}



-(NSRect)_upArrowFrameForStepperFrame:(NSRect)frame {
    int half = frame.size.height/2;
    if (![_controlView isFlipped])
        frame.origin.y += half;
    frame.size.height = half;
    return frame;
}

-(NSRect)_downArrowFrameForStepperFrame:(NSRect)frame {
    int half = frame.size.height/2;
    frame.size.height = half;
    if ([_controlView isFlipped])
        frame.origin.y += half;
    return frame;
}

/* The calendar style: the month and year between two arrows, the days'
   initials, and six weeks of days; the chosen day in the accent color,
   today's number in it. Laid out from the top whatever the view's
   flipping. */

#define CALENDAR_HEADER 24
#define CALENDAR_WEEKDAYS 18

static NSRect calendarBand(NSRect frame,BOOL flipped,CGFloat top,CGFloat height){
   NSRect result=frame;

   result.size.height=height;
   result.origin.y=flipped?NSMinY(frame)+top:NSMaxY(frame)-top-height;
   return result;
}

-(NSRect)_calendarFrameForFrame:(NSRect)frame {
   return NSInsetRect(frame,1,1);
}

-(NSRect)_calendarArrowFrame:(NSRect)frame flipped:(BOOL)flipped next:(BOOL)next {
   NSRect header=calendarBand([self _calendarFrameForFrame:frame],flipped,0,CALENDAR_HEADER);

   header.size.width=CALENDAR_HEADER;
   if(next)
    header.origin.x=NSMaxX([self _calendarFrameForFrame:frame])-CALENDAR_HEADER;
   return header;
}

/* The days' grid: 7 columns, 6 rows. */
-(NSRect)_calendarDayFrame:(NSRect)frame flipped:(BOOL)flipped row:(NSInteger)row column:(NSInteger)column {
   NSRect  calendar=[self _calendarFrameForFrame:frame];
   CGFloat top=CALENDAR_HEADER+CALENDAR_WEEKDAYS;
   CGFloat width=floor(NSWidth(calendar)/7),height=floor((NSHeight(calendar)-top)/6);
   NSRect  result=calendarBand(calendar,flipped,top+row*height,height);

   result.origin.x=NSMinX(calendar)+floor((NSWidth(calendar)-width*7)/2)+column*width;
   result.size.width=width;
   return result;
}

/* The first of the shown month, the column it falls in, and the month's days. */
-(NSDate *)_calendarFirstOfMonthColumn:(NSInteger *)column days:(NSInteger *)days {
   NSDate           *date=([self dateValue]!=nil)?[self dateValue]:[NSDate date];
   NSDateComponents *components=[_calendar components:NSYearCalendarUnit|NSMonthCalendarUnit fromDate:date];
   NSDate           *first;
   NSInteger         weekday;

   [components setDay:1];
   first=[_calendar dateFromComponents:components];
   weekday=[[_calendar components:NSWeekdayCalendarUnit fromDate:first] weekday];
   *column=(weekday-(NSInteger)[_calendar firstWeekday]+7)%7;
   *days=[_calendar rangeOfUnit:NSDayCalendarUnit inUnit:NSMonthCalendarUnit forDate:first].length;
   return first;
}

static void drawCentered(NSString *string,NSRect rect,NSDictionary *attributes){
   NSSize size=[string sizeWithAttributes:attributes];

   [string drawAtPoint:NSMakePoint(floor(NSMidX(rect)-size.width/2),floor(NSMidY(rect)-size.height/2)) withAttributes:attributes];
}

-(void)_drawCalendarWithFrame:(NSRect)frame inView:(NSView *)view {
   BOOL              flipped=[view isFlipped];
   NSRect            calendar=[self _calendarFrameForFrame:frame];
   NSFont           *font=([self font]!=nil)?[self font]:[NSFont systemFontOfSize:0];
   NSFont           *small=[NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
   NSColor          *text=[self isEnabled]?[self textColor]:[NSColor disabledControlTextColor];
   NSDateFormatter  *formatter=[[[NSDateFormatter alloc] init] autorelease];
   NSInteger         column,days,day,today=0,chosen=0;
   NSDate           *first=[self _calendarFirstOfMonthColumn:&column days:&days];
   NSDateComponents *month=[_calendar components:NSYearCalendarUnit|NSMonthCalendarUnit fromDate:first];
   NSArray          *weekdays;
   NSString         *title;

   if(_locale!=nil)
    [formatter setLocale:_locale];
   [formatter setCalendar:_calendar];

   if(_drawsBackground){
    [[self backgroundColor] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(calendar,1,1) xRadius:4 yRadius:4] fill];
   }
   [[view graphicsStyle] drawTextFieldBorderInRect:calendar bezeledNotLine:YES];

   /* The month and its arrows. */
   title=[NSString stringWithFormat:@"%@ %ld",[[formatter standaloneMonthSymbols] objectAtIndex:[month month]-1],(long)[month year]];
   drawCentered(title,calendarBand(calendar,flipped,0,CALENDAR_HEADER),
      [NSDictionary dictionaryWithObjectsAndKeys:[NSFont boldSystemFontOfSize:[font pointSize]],NSFontAttributeName,text,NSForegroundColorAttributeName,nil]);
   drawCentered(@"\u2039",[self _calendarArrowFrame:frame flipped:flipped next:NO],
      [NSDictionary dictionaryWithObjectsAndKeys:font,NSFontAttributeName,text,NSForegroundColorAttributeName,nil]);
   drawCentered(@"\u203A",[self _calendarArrowFrame:frame flipped:flipped next:YES],
      [NSDictionary dictionaryWithObjectsAndKeys:font,NSFontAttributeName,text,NSForegroundColorAttributeName,nil]);

   /* The days' initials, from the calendar's first weekday. */
   weekdays=[formatter veryShortStandaloneWeekdaySymbols];
   if([weekdays count]!=7)
    weekdays=[formatter shortWeekdaySymbols];
   if([weekdays count]==7){
    NSDictionary *attributes=[NSDictionary dictionaryWithObjectsAndKeys:small,NSFontAttributeName,[NSColor secondaryLabelColor],NSForegroundColorAttributeName,nil];
    NSInteger     i;

    for(i=0;i<7;i++){
     NSRect rect=[self _calendarDayFrame:frame flipped:flipped row:0 column:i];

     rect=calendarBand(rect,flipped,0,NSHeight(rect));
     rect.origin.y=calendarBand(calendar,flipped,CALENDAR_HEADER,CALENDAR_WEEKDAYS).origin.y;
     rect.size.height=CALENDAR_WEEKDAYS;
     drawCentered([weekdays objectAtIndex:([_calendar firstWeekday]-1+i)%7],rect,attributes);
    }
   }

   /* Which day is chosen, and today if it is in this month. */
   if([self dateValue]!=nil)
    chosen=[[_calendar components:NSDayCalendarUnit fromDate:[self dateValue]] day];
   {
    NSDateComponents *now=[_calendar components:NSYearCalendarUnit|NSMonthCalendarUnit|NSDayCalendarUnit fromDate:[NSDate date]];

    if([now year]==[month year] && [now month]==[month month])
     today=[now day];
   }

   for(day=1;day<=days;day++){
    NSInteger cell=column+day-1;
    NSRect    rect=[self _calendarDayFrame:frame flipped:flipped row:cell/7 column:cell%7];
    NSColor  *color=text;

    if(day==chosen){
     CGFloat side=MIN(NSWidth(rect),NSHeight(rect))-2;
     NSRect  dot=NSMakeRect(floor(NSMidX(rect)-side/2),floor(NSMidY(rect)-side/2),side,side);

     [[NSColor controlAccentColor] setFill];
     [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
     color=[NSColor alternateSelectedControlTextColor];
    }
    else if(day==today)
     color=[NSColor controlAccentColor];
    drawCentered([NSString stringWithFormat:@"%ld",(long)day],rect,
       [NSDictionary dictionaryWithObjectsAndKeys:font,NSFontAttributeName,color,NSForegroundColorAttributeName,nil]);
   }
}

/* A click on an arrow shows the month before or after (the day kept, or
   the month's last); on a day, chooses it. */
-(void)_trackCalendarAt:(NSPoint)point inView:(NSView *)view {
   NSRect            frame=[view bounds];
   BOOL              flipped=[view isFlipped];
   NSDate           *date=([self dateValue]!=nil)?[self dateValue]:[NSDate date];
   NSInteger         column,days,day;
   NSDateComponents *components;

   [self _calendarFirstOfMonthColumn:&column days:&days];

   if(NSMouseInRect(point,[self _calendarArrowFrame:frame flipped:flipped next:NO],flipped) ||
      NSMouseInRect(point,[self _calendarArrowFrame:frame flipped:flipped next:YES],flipped)){
    NSDateComponents *step=[[[NSDateComponents alloc] init] autorelease];

    [step setMonth:NSMouseInRect(point,[self _calendarArrowFrame:frame flipped:flipped next:YES],flipped)?1:-1];
    [self setDateValue:[_calendar dateByAddingComponents:step toDate:date options:0]];
    return;
   }

   for(day=1;day<=days;day++){
    NSInteger cell=column+day-1;

    if(NSMouseInRect(point,[self _calendarDayFrame:frame flipped:flipped row:cell/7 column:cell%7],flipped)){
     components=[_calendar components:NSYearCalendarUnit|NSMonthCalendarUnit|NSDayCalendarUnit|NSHourCalendarUnit|NSMinuteCalendarUnit|NSSecondCalendarUnit fromDate:date];
     [components setDay:day];
     [self setDateValue:[_calendar dateFromComponents:components]];
     return;
    }
   }
}

-(void)drawWithFrame:(NSRect)frame inView:(NSView *)view {
   NSRect stepper=[self _stepperFrameForFrame:frame];
   NSRect bezel=[self _bezelFrameForFrame:frame];

   _controlView=view;
   if(_style==NSClockAndCalendarDatePickerStyle){
    [self _drawCalendarWithFrame:frame inView:view];
    return;
   }

   if(_drawsBackground){
    [[self backgroundColor] setFill];
    NSRectFill(NSInsetRect(bezel,1,1));
   }
   [[_controlView graphicsStyle] drawTextFieldBorderInRect:bezel bezeledNotLine:YES];

   [self drawInteriorWithFrame:frame inView:view];

   if(_style==NSTextFieldDatePickerStyle)
    return;

   [[_controlView graphicsStyle] drawStepperButtonInRect:[self _upArrowFrameForStepperFrame:stepper] clipRect:stepper enabled:[self isEnabled] highlighted:_isUpHighlighted upNotDown:YES];
   [[_controlView graphicsStyle] drawStepperButtonInRect:[self _downArrowFrameForStepperFrame:stepper] clipRect:stepper enabled:[self isEnabled] highlighted:_isDownHighlighted upNotDown:NO];
}

-(void)selectComponentUnderPoint:(NSPoint)point inView:(NSView *)view {
   NSArray  *array=[self _attributedStrings];
   NSInteger i,count=[array count];
   NSRect    rects[count];
   NSRect    frame=[self _bezelFrameForFrame:[view bounds]];

   getRectsInFrameForAttributesStrings(rects,NSInsetRect(frame,2,0),array);

   for(i=0;i<count;i++){

    if(NSMouseInRect(point,rects[i],[view isFlipped])){
     NSAttributedString *string=[array objectAtIndex:i];
     NSDictionary       *attributes=[string attributesAtIndex:0 effectiveRange:NULL];
     NSUInteger          unit=[[attributes objectForKey:@"_unit"] unsignedIntegerValue];

     if(unit!=0){
      _selectedUnit=unit;
      break;
     }
    }
   }
}

-(void)incrementStepperUnderPoint:(NSPoint)point inView:(NSView *)view {
   NSRect stepper=[self _stepperFrameForFrame:[view bounds]];
   NSRect checkUp=[self _upArrowFrameForStepperFrame:stepper];
   NSRect checkDown=[self _downArrowFrameForStepperFrame:stepper];

   if(NSMouseInRect(point,checkUp,[view isFlipped]))
    [self _multiplyCurrentUnitBy:1 add:1];

   if(NSMouseInRect(point,checkDown,[view isFlipped]))
    [self _multiplyCurrentUnitBy:1 add:-1];
}

-(BOOL)startTrackingAt:(NSPoint)startPoint inView:(NSView *)view {
   startPoint=[view convertPoint:startPoint fromView:nil];
   if(_style==NSClockAndCalendarDatePickerStyle){
    [self _trackCalendarAt:startPoint inView:view];
    [view setNeedsDisplay:YES];
    return YES;
   }
   [self selectComponentUnderPoint:startPoint inView:view];
   [self incrementStepperUnderPoint:startPoint inView:view];
   return YES;
}

-(BOOL)continueTracking:(NSPoint)lastPoint at:(NSPoint)currentPoint inView:(NSView *)view {
   currentPoint=[view convertPoint:currentPoint fromView:nil];
   if(_style==NSClockAndCalendarDatePickerStyle)
    return YES;
   [self selectComponentUnderPoint:currentPoint inView:view];
   return YES;
}

-(void)insertText:(NSString *)text {
   NSInteger i,length=[text length];
   unichar   buffer[length];

   [text getCharacters:buffer];

   for(i=0;i<length;i++){
    unichar check=buffer[i];

    if(check>='0' && check<='9')
     [self _multiplyCurrentUnitBy:10 add:check-'0'];
   }
}

@end
