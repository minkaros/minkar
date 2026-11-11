/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import "NSLayoutInternal.h"
#import <AppKit/NSWindow.h>
#import "NSLayoutSolver.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* A layout pass: every view of the subtree (and every layout guide) gets
 * four variables, its left, top, width and height in the root's top-down
 * coordinates; the other attributes are expressions of them. The solver
 * gets:
 *   - the active constraints installed on the subtree's views;
 *   - for a view that translates its autoresizing mask: its mask as
 *     constraints on its superview's variables, required (fixed margins
 *     and sizes stay, flexible ones share what the superview gains or
 *     loses, as autoresizing does);
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
   /* What each required constraint was and its variables, for the log of
      one that can't be met: kept as plain C, as most passes never need it. */
   struct requiredEntry { id what; int count; int variables[8]; } *required;
   NSUInteger      requiredCount,requiredCapacity;
   NSHashTable    *needed;     // the views the solver needs (see findNeeded)
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
   free(required);
   if(needed!=NULL)
    NSFreeHashTable(needed);
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
   static int debug=-1;

   if(debug<0)
    debug=(getenv("NSLayoutDebug")!=NULL);
   if(debug){
    int i;
    BOOL odd=!isfinite(e.constant) || fabs(e.constant)>1e6;

    for(i=0;i<e.count;i++)
     if(!isfinite(e.coefficients[i]) || fabs(e.coefficients[i])>1e6)
      odd=YES;
    if(odd)
     NSLog(@"layout: an out of range term: constant %g, strength %g, first coefficient %g",e.constant,strength,e.count>0?e.coefficients[0]:0);
   }
   return NSLayoutSolverAdd(pass->solver,e.count,e.variables,e.coefficients,e.constant,op,strength);
}

/* What a required constraint is, as the log says it: a constraint, or a
   view (its autoresizing mask), or the text given. Made only for a log. */
static NSString *describeReason(id what) {
   if([what isKindOfClass:[NSView class]]){
    NSView *view=what;

    return [NSString stringWithFormat:@"the autoresizing mask %#lx of %@, frame %@",(unsigned long)[view autoresizingMask],view,NSStringFromRect([view frame])];
   }
   return [what description];
}

/* A required constraint that can't be met: logged with the required
 * constraints already in the pass that share a variable with it (the ones
 * it most likely fights), as the reference lists them. */
static void addRequired(NSLayoutPass *pass,expression e,int op,id what) {
   if(add(pass,e,op,NSLayoutSolverRequired())==NSLayoutSolverUnsatisfiable){
    NSMutableString *list=[NSMutableString string];
    NSUInteger       n;
    int              i,j;

    for(n=0;n<pass->requiredCount;n++){
     struct requiredEntry *entry=&pass->required[n];
     BOOL                  shared=NO;

     for(i=0;i<e.count && !shared;i++)
      for(j=0;j<entry->count && !shared;j++)
       shared=(entry->variables[j]==e.variables[i]);
     if(shared)
      [list appendFormat:@"    \"%@\",\n",describeReason(entry->what)];
    }
    [list appendFormat:@"    \"%@\"\n",describeReason(what)];
    NSLog(@"Unable to simultaneously satisfy constraints.\n\tProbably at least one of the constraints in the following list is one you don't want.\n(\n%@)\n\nWill attempt to recover by breaking\n%@",list,describeReason(what));
    add(pass,e,op,NSLayoutSolverStrength(999.999));
   }
   else if(what!=nil){
    struct requiredEntry *entry;

    if(pass->requiredCount==pass->requiredCapacity){
     pass->requiredCapacity=(pass->requiredCapacity==0)?64:pass->requiredCapacity*2;
     pass->required=realloc(pass->required,sizeof(struct requiredEntry)*pass->requiredCapacity);
    }
    entry=&pass->required[pass->requiredCount++];
    entry->what=what;
    entry->count=e.count;
    memcpy(entry->variables,e.variables,sizeof(int)*e.count);
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

   /* A view that translates its autoresizing mask is placed by it, all
      required (the root by its own frame): staying put and a size of at
      least zero would only add rows to the solver, most of a window's. */
   if(item!=pass->root && [item isKindOfClass:[NSView class]] && [(NSView *)item translatesAutoresizingMaskIntoConstraints])
    return;

   term(&e,first+WIDTH,1);
   add(pass,e,NSLayoutSolverGreaterOrEqual,NSLayoutSolverRequired());
   e.variables[0]=first+HEIGHT;
   add(pass,e,NSLayoutSolverGreaterOrEqual,NSLayoutSolverRequired());

   fix(pass,first+LEFT,frame.origin.x,stayStrength,nil);
   fix(pass,first+TOP,frame.origin.y,stayStrength,nil);
   fix(pass,first+WIDTH,frame.size.width,stayStrength,nil);
   fix(pass,first+HEIGHT,frame.size.height,stayStrength,nil);
}

/* Which views the solver needs: those laid out by constraints or with
   constraints or guides of their own, those a constraint names, and their
   superviews up to the root. A subtree of views placed by their frames
   alone keeps its autoresizing and stays out: a table's hundreds of rows
   made every pass cost seconds, the solver taking them in one by one. */
static BOOL findNeeded(NSView *view,NSHashTable *needed,NSHashTable *named) {
   BOOL uses=![view translatesAutoresizingMaskIntoConstraints] || [[view constraints] count]>0 || [[view layoutGuides] count]>0;

   for(NSLayoutConstraint *constraint in [view constraints]){
    id items[2]={ [constraint firstItem],[constraint secondItem] };
    int i;

    for(i=0;i<2;i++){
     id item=items[i];

     if([item isKindOfClass:[NSLayoutGuide class]])
      item=[(NSLayoutGuide *)item owningView];
     if([item isKindOfClass:[NSView class]])
      NSHashInsert(named,item);
    }
   }
   for(NSView *subview in [view subviews])
    if(findNeeded(subview,needed,named))
     uses=YES;
   if(uses)
    NSHashInsert(needed,view);
   return uses;
}

static void collect(NSLayoutPass *pass,NSView *view) {
   NSRect frame;

   if(view!=pass->root && pass->needed!=NULL && NSHashGet(pass->needed,view)==NULL)
    return;
   frame=rootFrame(view,pass->root);

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

/* One axis of an autoresizing mask as constraints: the view's position and
 * size (variables pos and size) from its superview's (spos, ssize), given
 * the current offset and length in the superview, the superview's length
 * and which of the leading margin, the size and the trailing margin are
 * flexible. */
static void translateAxis(NSLayoutPass *pass,int pos,int size,int spos,int ssize,double offset,double length,
                          double superLength,BOOL leadingFlexible,BOOL sizeFlexible,BOOL trailingFlexible,id what) {
   double     trailing=superLength-offset-length;
   expression e;

   if(!leadingFlexible && !sizeFlexible && !trailingFlexible)
    trailingFlexible=YES;   // a fixed frame keeps its leading margin

   if(!sizeFlexible){
    memset(&e,0,sizeof(e)); term(&e,size,1); e.constant=-length;
    addRequired(pass,e,NSLayoutSolverEqual,what);
    memset(&e,0,sizeof(e)); term(&e,pos,1); term(&e,spos,-1);
    if(!leadingFlexible)
     e.constant=-offset;
    else if(!trailingFlexible){
     term(&e,ssize,-1); e.constant=trailing+length;
    }
    else {
     double share=(superLength-length!=0)?offset/(superLength-length):0.5;
     term(&e,ssize,-share); e.constant=share*length;
    }
    addRequired(pass,e,NSLayoutSolverEqual,what);
    return;
   }

   if(!leadingFlexible && !trailingFlexible){
    memset(&e,0,sizeof(e)); term(&e,pos,1); term(&e,spos,-1); e.constant=-offset;
    addRequired(pass,e,NSLayoutSolverEqual,what);
    memset(&e,0,sizeof(e)); term(&e,size,1); term(&e,ssize,-1); e.constant=offset+trailing;
    addRequired(pass,e,NSLayoutSolverEqual,what);
   }
   else if(!leadingFlexible){
    double share=(length+trailing!=0)?length/(length+trailing):1;
    memset(&e,0,sizeof(e)); term(&e,pos,1); term(&e,spos,-1); e.constant=-offset;
    addRequired(pass,e,NSLayoutSolverEqual,what);
    memset(&e,0,sizeof(e)); term(&e,size,1); term(&e,ssize,-share); e.constant=share*offset;
    addRequired(pass,e,NSLayoutSolverEqual,what);
   }
   else if(!trailingFlexible){
    double share=(offset+length!=0)?length/(offset+length):1;
    memset(&e,0,sizeof(e)); term(&e,size,1); term(&e,ssize,-share); e.constant=share*trailing;
    addRequired(pass,e,NSLayoutSolverEqual,what);
    memset(&e,0,sizeof(e)); term(&e,pos,1); term(&e,size,1); term(&e,spos,-1); term(&e,ssize,-1); e.constant=trailing;
    addRequired(pass,e,NSLayoutSolverEqual,what);
   }
   else {
    double sizeShare=(superLength!=0)?length/superLength:1,offsetShare=(superLength!=0)?offset/superLength:0;
    memset(&e,0,sizeof(e)); term(&e,size,1); term(&e,ssize,-sizeShare);
    addRequired(pass,e,NSLayoutSolverEqual,what);
    memset(&e,0,sizeof(e)); term(&e,pos,1); term(&e,spos,-1); term(&e,ssize,-offsetShare);
    addRequired(pass,e,NSLayoutSolverEqual,what);
   }
}

// The view at the top of a window: the frame view, the content view's superview.
static BOOL isWindowFrameView(NSView *view) {
   NSWindow *window=[view window];

   return window!=nil && [view superview]==nil && [[window contentView] superview]==view;
}

static void addImplicit(NSLayoutPass *pass,NSView *view) {
   int v=firstVariable(pass,view);

   if(view==pass->root){
    fix(pass,v+LEFT,0,NSLayoutSolverRequired(),[NSString stringWithFormat:@"the root's x (%@)",view]);
    fix(pass,v+TOP,0,NSLayoutSolverRequired(),[NSString stringWithFormat:@"the root's y (%@)",view]);
    if(pass->fitting){
     double fit=NSLayoutSolverStrength(NSLayoutPriorityFittingSizeCompression);

     fix(pass,v+WIDTH,0,fit,nil);
     fix(pass,v+HEIGHT,0,fit,nil);
    }
    else if(isWindowFrameView(view)){
     // Minkar: a window's size stays put at NSLayoutPriorityWindowSizeStayPut,
     // as on the reference platform: content that needs more room (TextMate's
     // find panel) makes the window grow, instead of fighting a required size.
     double stay=NSLayoutSolverStrength(NSLayoutPriorityWindowSizeStayPut);

     fix(pass,v+WIDTH,[view frame].size.width,stay,nil);
     fix(pass,v+HEIGHT,[view frame].size.height,stay,nil);
    }
    else {
     fix(pass,v+WIDTH,[view frame].size.width,NSLayoutSolverRequired(),[NSString stringWithFormat:@"the root's width %g (%@)",[view frame].size.width,view]);
     fix(pass,v+HEIGHT,[view frame].size.height,NSLayoutSolverRequired(),[NSString stringWithFormat:@"the root's height %g (%@)",[view frame].size.height,view]);
    }
   }
   else if([view translatesAutoresizingMaskIntoConstraints]){
    NSRect      frame=rootFrame(view,pass->root);
    NSView     *superview=[view superview];
    int         s=(superview!=nil)?firstVariable(pass,superview):-1;
    id          what=view;   // described only if a log needs it

    if(s<0){
     fix(pass,v+LEFT,frame.origin.x,NSLayoutSolverRequired(),what);
     fix(pass,v+TOP,frame.origin.y,NSLayoutSolverRequired(),what);
     fix(pass,v+WIDTH,frame.size.width,NSLayoutSolverRequired(),what);
     fix(pass,v+HEIGHT,frame.size.height,NSLayoutSolverRequired(),what);
     return;
    }
    // Minkar: the mask relative to the superview, as the reference
    // translates it (the frame was fixed: a superview sized by
    // constraints could not resize its translating subviews).
    NSRect             parent=rootFrame(superview,pass->root);
    NSAutoresizingMaskOptions mask=[view autoresizingMask];
    BOOL               flipped=[superview isFlipped];
    BOOL               top=flipped?(mask&NSViewMinYMargin)!=0:(mask&NSViewMaxYMargin)!=0;
    BOOL               bottom=flipped?(mask&NSViewMaxYMargin)!=0:(mask&NSViewMinYMargin)!=0;

    translateAxis(pass,v+LEFT,v+WIDTH,s+LEFT,s+WIDTH,frame.origin.x-parent.origin.x,frame.size.width,
                  parent.size.width,(mask&NSViewMinXMargin)!=0,(mask&NSViewWidthSizable)!=0,(mask&NSViewMaxXMargin)!=0,what);
    translateAxis(pass,v+TOP,v+HEIGHT,s+TOP,s+HEIGHT,frame.origin.y-parent.origin.y,frame.size.height,
                  parent.size.height,top,(mask&NSViewHeightSizable)!=0,bottom,what);
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
   {
    NSHashTable *named=NSCreateHashTable(NSNonOwnedPointerHashCallBacks,0);
    NSHashEnumerator state;
    NSView *view;

    pass->needed=NSCreateHashTable(NSNonOwnedPointerHashCallBacks,0);
    findNeeded(root,pass->needed,named);
    state=NSEnumerateHashTable(named);
    while((view=NSNextHashEnumeratorItem(&state))!=nil)
     for(;view!=nil && view!=root;view=[view superview])
      NSHashInsert(pass->needed,view);
    NSEndHashTableEnumeration(&state);
    NSFreeHashTable(named);
   }
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
    NSTimeInterval start=[NSDate timeIntervalSinceReferenceDate];
    NSLayoutPass *pass=solve(root,NO);

    if(getenv("NSLayoutDebug")!=NULL)
     NSLog(@"layout solve: %lu views, %lu required constraints, %.1f ms",(unsigned long)[pass->views count],(unsigned long)pass->requiredCount,([NSDate timeIntervalSinceReferenceDate]-start)*1000);

    // The window to the size its content asked for, its top left kept.
    if(isWindowFrameView(root)){
     NSRect  frame=[[root window] frame];
     CGFloat width=value(pass,root,WIDTH),height=value(pass,root,HEIGHT);
     NSSize  size=[root frame].size;

     if(fabs(width-size.width)>=1 || fabs(height-size.height)>=1){
      frame.origin.y+=frame.size.height-(frame.size.height+height-size.height);
      frame.size.width+=width-size.width;
      frame.size.height+=height-size.height;
      [[root window] setFrame:frame display:YES];
     }
    }

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
