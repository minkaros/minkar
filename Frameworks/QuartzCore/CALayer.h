
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/CATransform3D.h>
#import <QuartzCore/CAAction.h>
#import <OpenGL/OpenGL.h>

@class CAAnimation, CALayerContext;

enum {
    kCALayerNotSizable = 0x00,
    kCALayerMinXMargin = 0x01,
    kCALayerWidthSizable = 0x02,
    kCALayerMaxXMargin = 0x04,
    kCALayerMinYMargin = 0x08,
    kCALayerHeightSizable = 0x10,
    kCALayerMaxYMargin = 0x20,
};

/* The edges drawn antialiased (edgeAntialiasingMask). */
typedef NS_OPTIONS(unsigned int, CAEdgeAntialiasingMask) {
    kCALayerLeftEdge = 1U << 0,
    kCALayerRightEdge = 1U << 1,
    kCALayerBottomEdge = 1U << 2,
    kCALayerTopEdge = 1U << 3,
};

/* The corners cornerRadius rounds (maskedCorners). */
typedef NS_OPTIONS(NSUInteger, CACornerMask) {
    kCALayerMinXMinYCorner = 1U << 0,
    kCALayerMaxXMinYCorner = 1U << 1,
    kCALayerMinXMaxYCorner = 1U << 2,
    kCALayerMaxXMaxYCorner = 1U << 3,
};

CA_EXPORT NSString *const kCAFilterLinear;
CA_EXPORT NSString *const kCAFilterNearest;
CA_EXPORT NSString *const kCAFilterTrilinear;

CA_EXPORT NSString *const kCAGravityCenter;
CA_EXPORT NSString *const kCAGravityTop;
CA_EXPORT NSString *const kCAGravityBottom;
CA_EXPORT NSString *const kCAGravityLeft;
CA_EXPORT NSString *const kCAGravityRight;
CA_EXPORT NSString *const kCAGravityTopLeft;
CA_EXPORT NSString *const kCAGravityTopRight;
CA_EXPORT NSString *const kCAGravityBottomLeft;
CA_EXPORT NSString *const kCAGravityBottomRight;
CA_EXPORT NSString *const kCAGravityResize;
CA_EXPORT NSString *const kCAGravityResizeAspect;
CA_EXPORT NSString *const kCAGravityResizeAspectFill;

@interface CALayer : NSObject {
    CALayerContext *_context;
    CALayer *_superlayer;
    NSArray *_sublayers;
    id _delegate;
    CGPoint _anchorPoint;
    CGPoint _position;
    CGRect _bounds;
    float _opacity;
    BOOL _opaque;
    id _contents;
    CATransform3D _transform;
    CATransform3D _sublayerTransform;
    NSString *_minificationFilter;
    NSString *_magnificationFilter;
    BOOL _needsDisplay;
    NSMutableDictionary *_animations;
    NSNumber *_textureId;
    CGColorRef _backgroundColor;
    CGColorRef _borderColor;
    CGColorRef _shadowColor;
    CGFloat _borderWidth;
    CGFloat _cornerRadius;
    CGFloat _contentsScale;
    CGFloat _zPosition;
    CGFloat _shadowRadius;
    float _shadowOpacity;
    CGSize _shadowOffset;
    CGRect _contentsRect;
    BOOL _masksToBounds;
    BOOL _hidden;
    BOOL _needsDisplayOnBoundsChange;
    BOOL _needsLayout;
    BOOL _doubleSided;
    BOOL _geometryFlipped;
    BOOL _allowsEdgeAntialiasing;
    unsigned _edgeAntialiasingMask;
    NSUInteger _maskedCorners;
    NSString *_name;
    NSString *_contentsGravity;
    CALayer *_mask;
    NSDictionary *_actions;
    NSDictionary *_style;
}

+ layer;

@property(readonly) CALayer *superlayer;
@property(copy) NSArray *sublayers;
@property(assign) id delegate;
@property CGPoint anchorPoint;
@property CGPoint position;
@property CGRect bounds;
@property CGRect frame;
@property float opacity;
@property(getter=isOpaque) BOOL opaque;
@property(retain) id contents;
//@property CATransform3D transform;
@property CATransform3D sublayerTransform;

@property(copy) NSString *minificationFilter;
@property(copy) NSString *magnificationFilter;

@property CGColorRef backgroundColor;
@property CGColorRef borderColor;
@property CGFloat borderWidth;
@property CGFloat cornerRadius;
@property BOOL masksToBounds;
/* Sublayers are laid out with y down; the layer's own contents are not. */
@property(getter=isGeometryFlipped) BOOL geometryFlipped;
@property BOOL contentsOpaque;
@property BOOL allowsEdgeAntialiasing;
@property CAEdgeAntialiasingMask edgeAntialiasingMask;
@property CACornerMask maskedCorners;
@property(getter=isHidden) BOOL hidden;
@property(getter=isDoubleSided) BOOL doubleSided;
@property(copy) NSString *name;
@property CGFloat zPosition;
@property CGFloat contentsScale;
@property(copy) NSString *contentsGravity;
@property CGRect contentsRect;
@property BOOL needsDisplayOnBoundsChange;
@property(retain) CALayer *mask;
@property(copy) NSDictionary *actions;
@property(copy) NSDictionary *style;
@property CGColorRef shadowColor;
@property float shadowOpacity;
@property CGSize shadowOffset;
@property CGFloat shadowRadius;

- init;

- (void)addSublayer:(CALayer *)layer;
- (void)insertSublayer:(CALayer *)layer atIndex:(unsigned)index;
- (void)insertSublayer:(CALayer *)layer below:(CALayer *)sibling;
- (void)insertSublayer:(CALayer *)layer above:(CALayer *)sibling;
- (void)setNeedsLayout;
- (BOOL)needsLayout;
- (void)layoutIfNeeded;
- (void)layoutSublayers;
- (CGPoint)convertPoint:(CGPoint)point fromLayer:(CALayer *)layer;
- (CGPoint)convertPoint:(CGPoint)point toLayer:(CALayer *)layer;
- (CGRect)convertRect:(CGRect)rect fromLayer:(CALayer *)layer;
- (CGRect)convertRect:(CGRect)rect toLayer:(CALayer *)layer;
- (BOOL)containsPoint:(CGPoint)point;
- (CALayer *)hitTest:(CGPoint)point;
/* Draws the layer and its sublayers into the context, in the layer's
   coordinates (the presentation is the model: no animation in flight is
   drawn). */
- (void)renderInContext:(CGContextRef)context;
- (id)presentationLayer;
- (id)modelLayer;
+ (id)defaultValueForKey:(NSString *)key;
+ (BOOL)needsDisplayForKey:(NSString *)key;
- (void)replaceSublayer:(CALayer *)layer with:(CALayer *)other;
- (void)display;
- (void)displayIfNeeded;
- (void)drawInContext:(CGContextRef)context;
- (BOOL)needsDisplay;
- (void)removeFromSuperlayer;
- (void)setNeedsDisplay;
- (void)setNeedsDisplayInRect:(CGRect)rect;

- (void)addAnimation:(CAAnimation *)animation forKey:(NSString *)key;
- (CAAnimation *)animationForKey:(NSString *)key;
- (void)removeAllAnimations;
- (void)removeAnimationForKey:(NSString *)key;
- (NSArray *)animationKeys;

- (id<CAAction>)actionForKey:(NSString *)key;

@end

@interface NSObject (CALayerDelegate)

- (void)displayLayer:(CALayer *)layer;
- (void)drawLayer:(CALayer *)layer inContext:(CGContextRef)context;
- (void)layoutSublayersOfLayer:(CALayer *)layer;

@end
