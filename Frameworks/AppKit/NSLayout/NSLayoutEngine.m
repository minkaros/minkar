/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import "NSLayoutInternal.h"
#import "NSLayoutSolver.h"
#include <math.h>

/* A layout pass: every view of the subtree (and every layout guide) gets
 * four variables, its left, top, width and height in the root's top-down
 * coordinates; the other attributes are expressions of them. The solver
 * gets:
 *   - the active constraints installed on the subtree's views;
 *   - for a view that translates its autoresizing mask: its frame, required
 *     (autoresizing has already moved it when its superview resized);
 *   - for a view with an intrinsic size: that size, with its hugging and
 *     compression resistance priorities;
 *   - the root's frame, required (or, for fittingSize, a size as small as
 *     the rest allows);
 *   - each item's current frame, very weakly: an underconstrained item
 *     stays where it is.
 * A required constraint that can't be met is broken (logged) and kept at
 * a priority just below required, as the reference does. Then the views
 * that don't translate their mask are given their solved frames, top
 * down, in their superviews' coordinates. */

enum { LEFT, TOP, WIDTH, HEIGHT };

typedef struct {
   int    count;
   int    variables[8];
   double coefficients[8];
   double constant;
} expression;

@interface NSLayoutPass : NSObject {
@public
   NSLayoutSolver *solver;
   NSMapTable     *items;      // item -> NSValue of its first variable
   NSMutableArray *views;
   NSView         *root;
   BOOL            fitting;
}
@end

@implementation NSLayoutPass

-init {
   solver=NSLayoutSolverCreate();
   items=NSCreateMapTable(NSNonOwnedPointerMapKeyCallBacks,NSObjectMapValueCallBacks,0);
   views=[[NSMutableArray alloc] init];
   return self;
}

-(void)dealloc {
   NSLayoutSolverFree(solver);
   NSFreeMapTable(items);
   [views release];
   [super dealloc];
}

@end

static const double stayStrength=0.1;

static int firstVariable(NSLayoutPass *pass,id item) {
   NSNumber *first=NSMapGet(pass->items,item);

   return (first!=nil)?[first intValue]:-1;
}

static void term(expression *e,int variable,double coefficient) {
   e->variables[e->count]=variable;
   e->coefficients[e->count]=coefficient;
   e->count++;
}

static int add(NSLayoutPass *pass,expression e,int op,double strength) {
   return NSLayoutSolverAdd(pass->solver,e.count,e.variables,e.coefficients,e.constant,op,strength);
}

static void addRequired(NSLayoutPass *pass,expression e,int op,id what) {
   if(add(pass,e,op,NSLayoutSolverRequired())==NSLayoutSolverUnsatisfiable){
    NSLog(@"Unable to simultaneously satisfy constraints. Will attempt to recover by breaking %@",what);
    add(pass,e,op,NSLayoutSolverStrength(999.999));
   }
}

// variable == value (op 0) at a strength.
static void fix(NSLayoutPass *pass,int variable,double value,double strength,id what) {
   expression e={0};

   term(&e,variable,1);
   e.constant=-value;
   if(strength>=NSLayoutSolverRequired())
    addRequired(pass,e,NSLayoutSolverEqual,what);
   else
    add(pass,e,NSLayoutSolverEqual,strength);
}

// The frame of a view, in the root's top-down coordinates.
static NSRect rootFrame(NSView *view,NSView *root) {
   NSRect   frame=[view frame];
   NSView  *superview=[view superview];

   if(view==root || superview==nil)
    return NSMakeRect(0,0,frame.size.width,frame.size.height);

   NSRect  parent=rootFrame(superview,root);
   NSRect  bounds=[superview bounds];
   CGFloat top=[superview isFlipped]?frame.origin.y-bounds.origin.y:NSMaxY(bounds)-NSMaxY(frame);

   return NSMakeRect(parent.origin.x+frame.origin.x-bounds.origin.x,parent.origin.y+top,frame.size.width,frame.size.height);
}

static void addItem(NSLayoutPass *pass,id item,NSRect frame) {
   int first=NSLayoutSolverNewVariable(pass->solver);
   expression e={0};

   NSLayoutSolverNewVariable(pass->solver);
   NSLayoutSolverNewVariable(pass->solver);
   NSLayoutSolverNewVariable(pass->solver);
   NSMapInsert(pass->items,item,[NSNumber numberWithInt:first]);

   term(&e,first+WIDTH,1);
   add(pass,e,NSLayoutSolverGreaterOrEqual,NSLayoutSolverRequired());
   e.variables[0]=first+HEIGHT;
   add(pass,e,NSLayoutSolverGreaterOrEqual,NSLayoutSolverRequired());

   fix(pass,first+LEFT,frame.origin.x,stayStrength,nil);
   fix(pass,first+TOP,frame.origin.y,stayStrength,nil);
   fix(pass,first+WIDTH,frame.size.width,stayStrength,nil);
   fix(pass,first+HEIGHT,frame.size.height,stayStrength,nil);
}

static void collect(NSLayoutPass *pass,NSView *view) {
   NSRect frame=rootFrame(view,pass->root);

   [pass->views addObject:view];
   addItem(pass,view,frame);
   for(NSLayoutGuide *guide in [view layoutGuides]){
    NSRect g=[guide frame];

    addItem(pass,guide,NSMakeRect(frame.origin.x+g.origin.x,frame.origin.y+g.origin.y,g.size.width,g.size.height));
   }
   for(NSView *subview in [view subviews])
    collect(pass,subview);
}

// An attribute of an item, times a factor, added to an expression.
static BOOL addAttribute(NSLayoutPass *pass,expression *e,id item,NSLayoutAttribute attribute,double factor) {
   int     v=firstVariable(pass,item);
   NSView *view=[item isKindOfClass:[NSView class]]?(NSView *)item:nil;

   if(v<0)
    return NO;
   switch(attribute){
    case NSLayoutAttributeLeft:
    case NSLayoutAttributeLeading:
     term(e,v+LEFT,factor);
     break;
    case NSLayoutAttributeRight:
    case NSLayoutAttributeTrailing:
     term(e,v+LEFT,factor); term(e,v+WIDTH,factor);
     break;
    case NSLayoutAttributeTop:
     term(e,v+TOP,factor);
     break;
    case NSLayoutAttributeBottom:
     term(e,v+TOP,factor); term(e,v+HEIGHT,factor);
     break;
    case NSLayoutAttributeWidth:
     term(e,v+WIDTH,factor);
     break;
    case NSLayoutAttributeHeight:
     term(e,v+HEIGHT,factor);
     break;
    case NSLayoutAttributeCenterX:
     term(e,v+LEFT,factor); term(e,v+WIDTH,factor*0.5);
     break;
    case NSLayoutAttributeCenterY:
     term(e,v+TOP,factor); term(e,v+HEIGHT,factor*0.5);
     break;
    case NSLayoutAttributeFirstBaseline:
     term(e,v+TOP,factor);
     e->constant+=factor*(view!=nil?[view firstBaselineOffsetFromTop]:0);
     break;
    case NSLayoutAttributeLastBaseline:
     term(e,v+TOP,factor); term(e,v+HEIGHT,factor);
     e->constant-=factor*(view!=nil?[view lastBaselineOffsetFromBottom]:0);
     break;
    default:
     return NO;
   }
   return YES;
}

// first.attribute (relation) multiplier * second.attribute + constant, as
// first - multiplier * second - constant (relation) 0.
static void addConstraint(NSLayoutPass *pass,NSLayoutConstraint *constraint) {
   expression e={0};
   id         second=[constraint secondItem];
   int        op;
   double     strength=NSLayoutSolverStrength([constraint priority]);

   if(!addAttribute(pass,&e,[constraint firstItem],[constraint firstAttribute],1))
    return;
   if(second!=nil && !addAttribute(pass,&e,second,[constraint secondAttribute],-[constraint multiplier]))
    return;
   e.constant-=[constraint constant];

   switch([constraint relation]){
    case NSLayoutRelationLessThanOrEqual: op=NSLayoutSolverLessOrEqual; break;
    case NSLayoutRelationGreaterThanOrEqual: op=NSLayoutSolverGreaterOrEqual; break;
    default: op=NSLayoutSolverEqual; break;
   }
   if(strength>=NSLayoutSolverRequired())
    addRequired(pass,e,op,constraint);
   else
    add(pass,e,op,strength);
}

static void addImplicit(NSLayoutPass *pass,NSView *view) {
   int v=firstVariable(pass,view);

   if(view==pass->root){
    fix(pass,v+LEFT,0,NSLayoutSolverRequired(),@"the root's x");
    fix(pass,v+TOP,0,NSLayoutSolverRequired(),@"the root's y");
    if(pass->fitting){
     double fit=NSLayoutSolverStrength(NSLayoutPriorityFittingSizeCompression);

     fix(pass,v+WIDTH,0,fit,nil);
     fix(pass,v+HEIGHT,0,fit,nil);
    }
    else {
     fix(pass,v+WIDTH,[view frame].size.width,NSLayoutSolverRequired(),@"the root's width");
     fix(pass,v+HEIGHT,[view frame].size.height,NSLayoutSolverRequired(),@"the root's height");
    }
   }
   else if([view translatesAutoresizingMaskIntoConstraints]){
    NSRect frame=rootFrame(view,pass->root);
    id     what=[NSString stringWithFormat:@"the autoresizing mask of %@",view];

    fix(pass,v+LEFT,frame.origin.x,NSLayoutSolverRequired(),what);
    fix(pass,v+TOP,frame.origin.y,NSLayoutSolverRequired(),what);
    fix(pass,v+WIDTH,frame.size.width,NSLayoutSolverRequired(),what);
    fix(pass,v+HEIGHT,frame.size.height,NSLayoutSolverRequired(),what);
    return;
   }

   NSSize  intrinsic=[view intrinsicContentSize];
   CGFloat sizes[2]={ intrinsic.width, intrinsic.height };
   int     o;

   for(o=0;o<2;o++){
    expression e={0};

    if(sizes[o]==NSViewNoIntrinsicMetric || (view==pass->root && !pass->fitting))
     continue;
    term(&e,v+(o==0?WIDTH:HEIGHT),1);
    e.constant=-sizes[o];
    add(pass,e,NSLayoutSolverLessOrEqual,NSLayoutSolverStrength([view contentHuggingPriorityForOrientation:o]));
    add(pass,e,NSLayoutSolverGreaterOrEqual,NSLayoutSolverStrength([view contentCompressionResistancePriorityForOrientation:o]));
   }
}

static NSLayoutPass *solve(NSView *root,BOOL fitting) {
   NSLayoutPass *pass=[[[NSLayoutPass alloc] init] autorelease];

   pass->root=root;
   pass->fitting=fitting;
   collect(pass,root);
   for(NSView *view in pass->views)
    addImplicit(pass,view);
   for(NSView *view in pass->views)
    for(NSLayoutConstraint *constraint in [view constraints])
     addConstraint(pass,constraint);
   NSLayoutSolverUpdate(pass->solver);
   return pass;
}

static CGFloat value(NSLayoutPass *pass,id item,int which) {
   return (CGFloat)round(NSLayoutSolverValue(pass->solver,firstVariable(pass,item)+which));
}

void NSLayoutEngineLayout(NSView *root) {
   @autoreleasepool {
    NSLayoutPass *pass=solve(root,NO);

    // Top down: a superview has its frame before its subviews are placed in it.
    for(NSView *view in pass->views){
     NSView *superview=[view superview];

     if(view!=root && superview!=nil && ![view translatesAutoresizingMaskIntoConstraints]){
      NSRect  bounds=[superview bounds];
      CGFloat width=value(pass,view,WIDTH),height=value(pass,view,HEIGHT);
      CGFloat x=value(pass,view,LEFT)-value(pass,superview,LEFT)+bounds.origin.x;
      CGFloat top=value(pass,view,TOP)-value(pass,superview,TOP);
      CGFloat y=[superview isFlipped]?bounds.origin.y+top:NSMaxY(bounds)-top-height;
      NSRect  frame=NSMakeRect(x,y,width,height);

      if(!NSEqualRects(frame,[view frame]))
       [view setFrame:frame];
     }
     for(NSLayoutGuide *guide in [view layoutGuides])
      [guide _setFrame:NSMakeRect(value(pass,guide,LEFT)-value(pass,view,LEFT),value(pass,guide,TOP)-value(pass,view,TOP),
       value(pass,guide,WIDTH),value(pass,guide,HEIGHT))];
    }
   }
}

NSSize NSLayoutEngineFittingSize(NSView *view) {
   NSSize result;

   @autoreleasepool {
    NSLayoutPass *pass=solve(view,YES);

    result=NSMakeSize(value(pass,view,WIDTH),value(pass,view,HEIGHT));
   }
   return result;
}
