#import <QuartzCore/CALayer.h>
#import <QuartzCore/CAAnimation.h>
#import <QuartzCore/CALayerContext.h>
#import <QuartzCore/CATransaction.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSTimer.h>
#import <Foundation/NSRunLoop.h>
#import <Foundation/NSString.h>
#import <objc/runtime.h>
#include <math.h>

NSString * const kCAFilterLinear=@"linear";
NSString * const kCAFilterNearest=@"nearest";
NSString * const kCAFilterTrilinear=@"trilinear";

NSString * const kCAGravityCenter=@"center";
NSString * const kCAGravityTop=@"top";
NSString * const kCAGravityBottom=@"bottom";
NSString * const kCAGravityLeft=@"left";
NSString * const kCAGravityRight=@"right";
NSString * const kCAGravityTopLeft=@"topLeft";
NSString * const kCAGravityTopRight=@"topRight";
NSString * const kCAGravityBottomLeft=@"bottomLeft";
NSString * const kCAGravityBottomRight=@"bottomRight";
NSString * const kCAGravityResize=@"resize";
NSString * const kCAGravityResizeAspect=@"resizeAspect";
NSString * const kCAGravityResizeAspectFill=@"resizeAspectFill";

@interface CALayer (CALayerAnimationRuns)
-(void)_animationRunDidEnd:(id)run;
@end

/* An animation a layer runs: its delegate hears it start, and stop when
 * its active time is over (finished) or when it is removed before
 * (not finished); one removed on completion goes then. The layer is kept
 * while its animations run. */
@interface CALayerAnimationRun : NSObject {
@public
   CALayer     *_layer;
   NSString    *_key;
   CAAnimation *_animation;
   NSTimer     *_timer;
   BOOL         _started,_stopped;
}
@end

@implementation CALayerAnimationRun

-(void)dealloc {
   [_layer release];
   [_key release];
   [_animation release];
   [super dealloc];
}

-(void)start {
   id delegate=[_animation delegate];

   if(_started || _stopped)
    return;
   _started=YES;
   if([delegate respondsToSelector:@selector(animationDidStart:)])
    [delegate animationDidStart:_animation];
}

-(void)stopFinished:(BOOL)finished {
   id delegate=[_animation delegate];

   if(_stopped)
    return;
   [self start];
   _stopped=YES;
   [_timer invalidate];
   _timer=nil;
   if([delegate respondsToSelector:@selector(animationDidStop:finished:)])
    [delegate animationDidStop:_animation finished:finished];
}

-(void)timerFired:(NSTimer *)timer {
   [[self retain] autorelease];
   _timer=nil;
   [_layer _animationRunDidEnd:self];
}

@end

static char animationRunsKey;

static NSMutableDictionary *animationRuns(CALayer *self) {
   NSMutableDictionary *runs=objc_getAssociatedObject(self,&animationRunsKey);

   if(runs==nil){
    runs=[NSMutableDictionary dictionary];
    objc_setAssociatedObject(self,&animationRunsKey,runs,OBJC_ASSOCIATION_RETAIN);
   }
   return runs;
}

/* How long an animation runs from now: its duration (the default when it
 * has none), repeated, reversed and scaled by its speed, after its begin
 * time; a paused one does not end. */
static CFTimeInterval animationActiveTime(CAAnimation *animation) {
   CFTimeInterval duration=[animation duration],total;
   float          repeatCount=[animation repeatCount],speed=[animation speed];

   if(duration<=0)
    duration=0.25;
   if([animation repeatDuration]>0)
    total=[animation repeatDuration];
   else if(repeatCount>0)
    total=duration*repeatCount;
   else
    total=duration;
   if([animation autoreverses])
    total*=2;
   if(speed<=0 || isinf(total))
    return INFINITY;
   total/=speed;
   if([animation beginTime]>0){
    CFTimeInterval now=CACurrentMediaTime();

    if([animation beginTime]>now)
     total+=[animation beginTime]-now;
   }
   return total;
}

@implementation CALayer

+layer {
   return [[[self alloc] init] autorelease];
}


-(CALayerContext *)_context {
   return _context;
}

-(void)_setContext:(CALayerContext *)context {
   if(_context!=context){
    [_context deleteTextureId:_textureId];
    [_textureId release];
    _textureId=nil;
   }
   
   _context=context;
   [_sublayers makeObjectsPerformSelector:@selector(_setContext:) withObject:context];
}

-(CALayer *)superlayer {
   return _superlayer;
}

-(NSArray *)sublayers {
   return _sublayers;
}

-(void)setSublayers:(NSArray *)sublayers {
   sublayers=[sublayers copy];
   [_sublayers release];
   _sublayers=sublayers;
   [_sublayers makeObjectsPerformSelector:@selector(_setSuperLayer:) withObject:self];
   [_sublayers makeObjectsPerformSelector:@selector(_setContext:) withObject:_context];
}

-(id)delegate {
   return _delegate;
}

-(void)setDelegate:value {
   _delegate=value;
}

-(CGPoint)anchorPoint {
   return _anchorPoint;
}

-(void)setAnchorPoint:(CGPoint)value {
   _anchorPoint=value;
}

-(CGPoint)position {
   return _position;
}

/* Implicit animations run where a render context draws the layer (as
   before explicit animations ran everywhere). */
-(void)setPosition:(CGPoint)value {
   CAAnimation *animation=[self animationForKey:@"position"];
      
   if(_context!=nil && animation==nil && ![CATransaction disableActions]){
    id action=[self actionForKey:@"position"];
    
    if(action!=nil)
     [self addAnimation:action forKey:@"position"];
   }
   
   _position=value;
}

-(CGRect)bounds {
   return _bounds;
}

-(void)setBounds:(CGRect)value {
   CAAnimation *animation=[self animationForKey:@"bounds"];
   
   if(_context!=nil && animation==nil && ![CATransaction disableActions]){
    id action=[self actionForKey:@"bounds"];

    if(action!=nil)
     [self addAnimation:action forKey:@"bounds"];
   }
   
   if(_needsDisplayOnBoundsChange && !CGSizeEqualToSize(_bounds.size,value.size))
    _needsDisplay=YES;
   if(!CGSizeEqualToSize(_bounds.size,value.size))
    _needsLayout=YES;
   _bounds=value;
}

-(CGRect)frame {
   CGRect result;
   
   result.size=_bounds.size;
   result.origin.x=_position.x-result.size.width*_anchorPoint.x;
   result.origin.y=_position.y-result.size.height*_anchorPoint.y;
   
   return result;
}

-(void)setFrame:(CGRect)value {

   CGPoint position;
   
   position.x=value.origin.x+value.size.width*_anchorPoint.x;
   position.y=value.origin.y+value.size.height*_anchorPoint.y;
   
   [self setPosition:position];
   
   CGRect bounds=_bounds;

   bounds.size=value.size;
      
   [self setBounds:bounds];
}

-(float)opacity {
   return _opacity;
}

-(void)setOpacity:(float)value {
   CAAnimation *animation=[self animationForKey:@"opacity"];
   
   if(_context!=nil && animation==nil && ![CATransaction disableActions]){
    id action=[self actionForKey:@"opacity"];

    if(action!=nil)
     [self addAnimation:action forKey:@"opacity"];
   }

   _opacity=value;
}

-(BOOL)opaque {
   return _opaque;
}

-(void)setOpaque:(BOOL)value {
   _opaque=value;
}

-(id)contents {
   return _contents;
}

-(void)setContents:(id)value {
   value=[value retain];
   [_contents release];
   _contents=value;
}

-(CATransform3D)transform {
   return _transform;
}

-(void)setTransform:(CATransform3D)value {
   _transform=value;
}

-(CATransform3D)sublayerTransform {
   return _sublayerTransform;
}

-(void)setSublayerTransform:(CATransform3D)value {
   _sublayerTransform=value;
}

-(NSString *)minificationFilter {
   return _minificationFilter;
}

-(void)setMinificationFilter:(NSString *)value {
   value=[value copy];
   [_minificationFilter release];
   _minificationFilter=value;
}

-(NSString *)magnificationFilter {
   return _magnificationFilter;
}

-(void)setMagnificationFilter:(NSString *)value {
   value=[value copy];
   [_magnificationFilter release];
   _magnificationFilter=value;
}

-init {
   _superlayer=nil;
   _sublayers=[NSArray new];
   _delegate=nil;
   _anchorPoint=CGPointMake(0.5,0.5);
   _position=CGPointZero;
   _bounds=CGRectZero;
   _opacity=1.0;
   _opaque=YES;
   _contents=nil;
   _transform=CATransform3DIdentity;
   _sublayerTransform=CATransform3DIdentity;
   _minificationFilter=kCAFilterLinear;
   _magnificationFilter=kCAFilterLinear;
   _animations=[[NSMutableDictionary alloc] init];
   _contentsScale=1;
   _contentsRect=CGRectMake(0,0,1,1);
   _contentsGravity=[kCAGravityResize copy];
   _shadowOffset=CGSizeMake(0,-3);
   _shadowRadius=3;
   _doubleSided=YES;
   return self;
}

-(void)dealloc {
   CGColorRelease(_backgroundColor);
   CGColorRelease(_borderColor);
   CGColorRelease(_shadowColor);
   [_name release];
   [_contentsGravity release];
   [_mask release];
   [_actions release];
   [_style release];
   [_contents release];
   [_sublayers release];
   [_animations release];
   [_minificationFilter release];
   [_magnificationFilter release];
   [super dealloc];
}

-(void)_setSuperLayer:(CALayer *)parent {
   _superlayer=parent;
}

-(void)_removeSublayer:(CALayer *)child {
   NSMutableArray *layers=[_sublayers mutableCopy];
   [layers removeObjectIdenticalTo:child];
   [self setSublayers:layers];
   [layers release];
}

-(void)addSublayer:(CALayer *)layer {
   [self setSublayers:[_sublayers arrayByAddingObject:layer]];
}

-(void)replaceSublayer:(CALayer *)layer with:(CALayer *)other {
   NSMutableArray *layers=[_sublayers mutableCopy];
   NSUInteger      index=[_sublayers indexOfObjectIdenticalTo:layer];
   
   [layers replaceObjectAtIndex:index withObject:other];
   
   [self setSublayers:layers];
   [layers release];
   
   layer->_superlayer=nil;
}

/* The layer's content: the delegate's displayLayer:, else a bitmap of
   the bounds (times contentsScale) drawn by drawInContext:. */
-(void)display {
   if([_delegate respondsToSelector:@selector(displayLayer:)]){
    [_delegate displayLayer:self];
    _needsDisplay=NO;
    return;
   }
   size_t width=(size_t)ceil(_bounds.size.width*_contentsScale),height=(size_t)ceil(_bounds.size.height*_contentsScale);

   _needsDisplay=NO;
   if(width==0 || height==0){
    [self setContents:nil];
    return;
   }
   CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
   CGContextRef    context=CGBitmapContextCreate(NULL,width,height,8,width*4,space,kCGImageAlphaPremultipliedLast);
   CGColorSpaceRelease(space);
   if(context==NULL)
    return;
   CGContextScaleCTM(context,_contentsScale,_contentsScale);
   CGContextTranslateCTM(context,-_bounds.origin.x,-_bounds.origin.y);
   [self drawInContext:context];
   CGImageRef image=CGBitmapContextCreateImage(context);
   CGContextRelease(context);
   [self setContents:(id)image];
   CGImageRelease(image);
}

-(void)displayIfNeeded {
   if(_needsDisplay)
    [self display];
}

-(void)drawInContext:(CGContextRef)context {
   if([_delegate respondsToSelector:@selector(drawLayer:inContext:)])
    [_delegate drawLayer:self inContext:context];
}

-(BOOL)needsDisplay {
   return _needsDisplay;
}

-(void)removeFromSuperlayer {
   [_superlayer _removeSublayer:self];
   _superlayer=nil;
   [self _setContext:nil];
}

-(void)setNeedsDisplay {
   _needsDisplay=YES;
}

-(void)setNeedsDisplayInRect:(CGRect)rect {
   _needsDisplay=YES;
}

#define COLOR_PROPERTY(getter,setter,ivar) \
   -(CGColorRef)getter { return ivar; } \
   -(void)setter:(CGColorRef)value { value=CGColorRetain(value); CGColorRelease(ivar); ivar=value; }

COLOR_PROPERTY(backgroundColor,setBackgroundColor,_backgroundColor)
COLOR_PROPERTY(borderColor,setBorderColor,_borderColor)
COLOR_PROPERTY(shadowColor,setShadowColor,_shadowColor)

#define VALUE_PROPERTY(type,getter,setter,ivar) \
   -(type)getter { return ivar; } \
   -(void)setter:(type)value { ivar=value; }

VALUE_PROPERTY(CGFloat,borderWidth,setBorderWidth,_borderWidth)
VALUE_PROPERTY(CGFloat,cornerRadius,setCornerRadius,_cornerRadius)
VALUE_PROPERTY(BOOL,masksToBounds,setMasksToBounds,_masksToBounds)
VALUE_PROPERTY(BOOL,isHidden,setHidden,_hidden)
VALUE_PROPERTY(BOOL,isDoubleSided,setDoubleSided,_doubleSided)
VALUE_PROPERTY(CGFloat,zPosition,setZPosition,_zPosition)
VALUE_PROPERTY(CGFloat,contentsScale,setContentsScale,_contentsScale)
VALUE_PROPERTY(CGRect,contentsRect,setContentsRect,_contentsRect)
VALUE_PROPERTY(BOOL,needsDisplayOnBoundsChange,setNeedsDisplayOnBoundsChange,_needsDisplayOnBoundsChange)
VALUE_PROPERTY(float,shadowOpacity,setShadowOpacity,_shadowOpacity)
VALUE_PROPERTY(CGSize,shadowOffset,setShadowOffset,_shadowOffset)
VALUE_PROPERTY(CGFloat,shadowRadius,setShadowRadius,_shadowRadius)

#define COPY_PROPERTY(type,getter,setter,ivar) \
   -(type)getter { return ivar; } \
   -(void)setter:(type)value { value=[value copy]; [ivar release]; ivar=value; }

COPY_PROPERTY(NSString *,name,setName,_name)
COPY_PROPERTY(NSString *,contentsGravity,setContentsGravity,_contentsGravity)
COPY_PROPERTY(NSDictionary *,actions,setActions,_actions)
COPY_PROPERTY(NSDictionary *,style,setStyle,_style)

-(CALayer *)mask { return _mask; }
-(void)setMask:(CALayer *)value {
   value=[value retain];
   [_mask release];
   _mask=value;
}

-(void)insertSublayer:(CALayer *)layer atIndex:(unsigned)index {
   NSMutableArray *layers=[_sublayers mutableCopy];

   [layer removeFromSuperlayer];
   [layers insertObject:layer atIndex:MIN((NSUInteger)index,[layers count])];
   [self setSublayers:layers];
   [layers release];
}

-(void)insertSublayer:(CALayer *)layer below:(CALayer *)sibling {
   NSUInteger index=[_sublayers indexOfObjectIdenticalTo:sibling];
   [self insertSublayer:layer atIndex:(index==NSNotFound)?0:(unsigned)index];
}

-(void)insertSublayer:(CALayer *)layer above:(CALayer *)sibling {
   NSUInteger index=[_sublayers indexOfObjectIdenticalTo:sibling];
   [self insertSublayer:layer atIndex:(index==NSNotFound)?(unsigned)[_sublayers count]:(unsigned)index+1];
}

-(void)setNeedsLayout {
   _needsLayout=YES;
}

-(BOOL)needsLayout {
   return _needsLayout;
}

-(void)layoutSublayers {
   if([_delegate respondsToSelector:@selector(layoutSublayersOfLayer:)])
    [_delegate layoutSublayersOfLayer:self];
}

-(void)layoutIfNeeded {
   if(_needsLayout){
    _needsLayout=NO;
    [self layoutSublayers];
   }
   for(CALayer *child in _sublayers)
    [child layoutIfNeeded];
}

/* The transform from this layer's coordinates to its superlayer's: the
   frame's origin, and the transform about the anchor point (its affine
   part). */
-(CGAffineTransform)_toSuperlayer {
   CGAffineTransform t=CGAffineTransformMakeTranslation(_position.x,_position.y);
   CGAffineTransform own=CGAffineTransformMake(_transform.m11,_transform.m12,_transform.m21,_transform.m22,_transform.m41,_transform.m42);

   t=CGAffineTransformConcat(own,t);
   t=CGAffineTransformTranslate(t,-_bounds.size.width*_anchorPoint.x-_bounds.origin.x,-_bounds.size.height*_anchorPoint.y-_bounds.origin.y);
   return t;
}

-(CGAffineTransform)_toRoot {
   CGAffineTransform t=CGAffineTransformIdentity;
   for(CALayer *layer=self;layer->_superlayer!=nil;layer=layer->_superlayer)
    t=CGAffineTransformConcat(t,[layer _toSuperlayer]);
   return t;
}

-(CGPoint)convertPoint:(CGPoint)point toLayer:(CALayer *)layer {
   CGPoint root=CGPointApplyAffineTransform(point,[self _toRoot]);
   return (layer==nil)?root:CGPointApplyAffineTransform(root,CGAffineTransformInvert([layer _toRoot]));
}

-(CGPoint)convertPoint:(CGPoint)point fromLayer:(CALayer *)layer {
   return (layer==nil)?CGPointApplyAffineTransform(point,CGAffineTransformInvert([self _toRoot])):[layer convertPoint:point toLayer:self];
}

-(CGRect)convertRect:(CGRect)rect toLayer:(CALayer *)layer {
   CGAffineTransform t=[self _toRoot];
   if(layer!=nil)
    t=CGAffineTransformConcat(t,CGAffineTransformInvert([layer _toRoot]));
   return CGRectApplyAffineTransform(rect,t);
}

-(CGRect)convertRect:(CGRect)rect fromLayer:(CALayer *)layer {
   if(layer==nil)
    return CGRectApplyAffineTransform(rect,CGAffineTransformInvert([self _toRoot]));
   return [layer convertRect:rect toLayer:self];
}

-(BOOL)containsPoint:(CGPoint)point {
   return CGRectContainsPoint(_bounds,point);
}

/* The deepest visible layer under the point (in the superlayer's
   coordinates), front sublayers first. */
-(CALayer *)hitTest:(CGPoint)point {
   if(_hidden || _opacity<=0)
    return nil;
   CGPoint local=CGPointApplyAffineTransform(point,CGAffineTransformInvert([self _toSuperlayer]));

   if(_masksToBounds && ![self containsPoint:local])
    return nil;
   for(CALayer *child in [_sublayers reverseObjectEnumerator]){
    CALayer *hit=[child hitTest:local];
    if(hit!=nil)
     return hit;
   }
   return [self containsPoint:local]?self:nil;
}

-(id)presentationLayer {
   return self;
}

-(id)modelLayer {
   return self;
}

+(id)defaultValueForKey:(NSString *)key {
   return nil;
}

+(BOOL)needsDisplayForKey:(NSString *)key {
   return NO;
}

static CGPathRef createRoundedPath(CGRect rect,CGFloat radius) {
   CGMutablePathRef path=CGPathCreateMutable();
   radius=MIN(radius,MIN(rect.size.width,rect.size.height)/2);
   if(radius<=0)
    CGPathAddRect(path,NULL,rect);
   else {
    CGFloat minX=CGRectGetMinX(rect),maxX=CGRectGetMaxX(rect),minY=CGRectGetMinY(rect),maxY=CGRectGetMaxY(rect);
    CGPathMoveToPoint(path,NULL,minX+radius,minY);
    CGPathAddArcToPoint(path,NULL,maxX,minY,maxX,maxY,radius);
    CGPathAddArcToPoint(path,NULL,maxX,maxY,minX,maxY,radius);
    CGPathAddArcToPoint(path,NULL,minX,maxY,minX,minY,radius);
    CGPathAddArcToPoint(path,NULL,minX,minY,maxX,minY,radius);
    CGPathCloseSubpath(path);
   }
   return path;
}

/* Where contents of the size go in the bounds, by contentsGravity (the
   layer's coordinates, y up). */
-(CGRect)_contentsFrameForSize:(CGSize)size {
   CGRect   b=_bounds;
   NSString *g=_contentsGravity;
   CGFloat  scale;

   if(g==nil || [g isEqualToString:kCAGravityResize] || size.width<=0 || size.height<=0)
    return b;
   if([g isEqualToString:kCAGravityResizeAspect] || [g isEqualToString:kCAGravityResizeAspectFill]){
    CGFloat sx=b.size.width/size.width,sy=b.size.height/size.height;
    scale=[g isEqualToString:kCAGravityResizeAspect]?MIN(sx,sy):MAX(sx,sy);
    size=CGSizeMake(size.width*scale,size.height*scale);
    return CGRectMake(CGRectGetMidX(b)-size.width/2,CGRectGetMidY(b)-size.height/2,size.width,size.height);
   }
   CGFloat x=CGRectGetMidX(b)-size.width/2,y=CGRectGetMidY(b)-size.height/2;
   if([g rangeOfString:@"eft"].location!=NSNotFound) x=CGRectGetMinX(b);
   if([g rangeOfString:@"ight"].location!=NSNotFound) x=CGRectGetMaxX(b)-size.width;
   if([g hasPrefix:@"top"]) y=CGRectGetMaxY(b)-size.height;
   if([g hasPrefix:@"bottom"]) y=CGRectGetMinY(b);
   return CGRectMake(x,y,size.width,size.height);
}

-(void)renderInContext:(CGContextRef)context {
   CGPathRef shape;

   if(_hidden || context==NULL)
    return;
   [self layoutIfNeeded];
   CGContextSaveGState(context);
   CGContextSetAlpha(context,_opacity);
   shape=createRoundedPath(_bounds,_cornerRadius);
   if(_masksToBounds){
    CGContextAddPath(context,shape);
    CGContextClip(context);
   }
   if(_backgroundColor!=NULL){
    CGContextSetFillColorWithColor(context,_backgroundColor);
    CGContextAddPath(context,shape);
    CGContextFillPath(context);
   }
   if(_contents!=nil && CFGetTypeID((CFTypeRef)_contents)==CGImageGetTypeID()){
    CGImageRef image=(CGImageRef)_contents;
    CGSize     size=CGSizeMake(CGImageGetWidth(image)/_contentsScale,CGImageGetHeight(image)/_contentsScale);
    CGContextDrawImage(context,[self _contentsFrameForSize:size],image);
   }
   else
    [self drawInContext:context];
   NSArray *ordered=[_sublayers sortedArrayUsingComparator:^NSComparisonResult(CALayer *a,CALayer *b){
      return (a->_zPosition<b->_zPosition)?NSOrderedAscending:(a->_zPosition>b->_zPosition)?NSOrderedDescending:NSOrderedSame;
   }];
   for(CALayer *child in ordered){
    CGContextSaveGState(context);
    CGContextConcatCTM(context,[child _toSuperlayer]);
    [child renderInContext:context];
    CGContextRestoreGState(context);
   }
   if(_borderWidth>0 && _borderColor!=NULL){
    CGPathRef inner=createRoundedPath(CGRectInset(_bounds,_borderWidth/2,_borderWidth/2),MAX(0,_cornerRadius-_borderWidth/2));
    CGContextSetStrokeColorWithColor(context,_borderColor);
    CGContextSetLineWidth(context,_borderWidth);
    CGContextAddPath(context,inner);
    CGContextStrokePath(context);
    CGPathRelease(inner);
   }
   CGPathRelease(shape);
   CGContextRestoreGState(context);
}

/* The layer keeps a copy; with no key it makes one. */
-(void)addAnimation:(CAAnimation *)animation forKey:(NSString *)key {
   CALayerAnimationRun *run;
   CFTimeInterval       active;

   if(animation==nil)
    return;
   animation=[[animation copy] autorelease];
   if(key==nil)
    key=[NSString stringWithFormat:@"_CAAnimation%p",animation];
   [self removeAnimationForKey:key];

   [_animations setObject:animation forKey:key];
   if(_context!=nil)
    [_context startTimerIfNeeded];

   run=[[[CALayerAnimationRun alloc] init] autorelease];
   run->_layer=[self retain];
   run->_key=[key copy];
   run->_animation=[animation retain];
   [animationRuns(self) setObject:run forKey:key];
   [run performSelector:@selector(start) withObject:nil afterDelay:0
       inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];

   active=animationActiveTime(animation);
   if(!isinf(active)){
    run->_timer=[NSTimer timerWithTimeInterval:active target:run selector:@selector(timerFired:) userInfo:nil repeats:NO];
    [[NSRunLoop mainRunLoop] addTimer:run->_timer forMode:NSRunLoopCommonModes];
   }
}

/* An animation's active time is over. */
-(void)_animationRunDidEnd:(CALayerAnimationRun *)run {
   NSMutableDictionary *runs=animationRuns(self);

   if([runs objectForKey:run->_key]!=run)
    return;
   [[run retain] autorelease];
   [runs removeObjectForKey:run->_key];
   if([run->_animation isRemovedOnCompletion] && [_animations objectForKey:run->_key]==run->_animation)
    [_animations removeObjectForKey:run->_key];
   [run stopFinished:YES];
}

-(CAAnimation *)animationForKey:(NSString *)key {
   return [_animations objectForKey:key];
}

-(void)removeAllAnimations {
   for(NSString *key in [_animations allKeys])
    [self removeAnimationForKey:key];
}

/* Removed before its active time is over, it did not finish. */
-(void)removeAnimationForKey:(NSString *)key {
   NSMutableDictionary *runs;
   CALayerAnimationRun *run;

   if(key==nil)
    return;
   runs=objc_getAssociatedObject(self,&animationRunsKey);
   run=[[[runs objectForKey:key] retain] autorelease];
   [_animations removeObjectForKey:key];
   if(run!=nil){
    [runs removeObjectForKey:key];
    [run stopFinished:NO];
   }
}

-(NSArray *)animationKeys {
   return [_animations allKeys];
}

-valueForKey:(NSString *)key {
// FIXME: KVC appears broken for structs

   if([key isEqualToString:@"bounds"])
    return [NSValue valueWithRect:_bounds];
   if([key isEqualToString:@"frame"])
    return [NSValue valueWithRect:[self frame]];
    
   return [super valueForKey:key];
}

-(id <CAAction>)actionForKey:(NSString *)key {
   CABasicAnimation *basic=[CABasicAnimation animationWithKeyPath:key];
   
   [basic setFromValue:[self valueForKey:key]];
   
   return basic;
}

-(NSNumber *)_textureId {
   return _textureId;
}

-(void)_setTextureId:(NSNumber *)value {
   value=[value copy];
   [_textureId release];
   _textureId=value;
}

@end
