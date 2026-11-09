#import <AppKit/NSViewController.h>
#import <AppKit/NSNibLoading.h>
#import <AppKit/NSNib.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSPopover.h>
#import <AppKit/NSLayoutConstraint.h>
#import <objc/runtime.h>

static char controllerKey;

/* The view keeps a weak link to its controller. */
NSViewController *_NSViewControllerForView(NSView *view) {
   return (view!=nil)?[(NSValue *)objc_getAssociatedObject(view,&controllerKey) nonretainedObjectValue]:nil;
}

@implementation NSViewController

-init {
   return [self initWithNibName:nil bundle:nil];
}

-initWithNibName:(NSString *)name bundle:(NSBundle *)bundle {
   _nibName=[name copy];
   _nibBundle=[bundle retain];
   return self;
}

-initWithCoder:(NSCoder *)coder {
   if([coder allowsKeyedCoding]){
    _nibName=[[coder decodeObjectForKey:@"NSNibName"] copy];
    _title=[[coder decodeObjectForKey:@"NSTitle"] copy];
    NSString *bundleIdentifier=[coder decodeObjectForKey:@"NSNibBundleIdentifier"];
    if(bundleIdentifier!=nil)
     _nibBundle=[[NSBundle bundleWithIdentifier:bundleIdentifier] retain];
   }

   return self;
}

-(void)dealloc {
   if(_view!=nil && _NSViewControllerForView(_view)==self)
    objc_setAssociatedObject(_view,&controllerKey,nil,OBJC_ASSOCIATION_RETAIN);
   for(NSViewController *child in _childViewControllers)
    child->_parentViewController=nil;
   [_childViewControllers release];
   [_presentedViewControllers release];
   [_nibName release];
   [_nibBundle release];
   [_representedObject release];
   [_title release];
   [_view release];
   [_identifier release];
   [_presentation release];
   [super dealloc];
}

-(NSString *)nibName {
   return _nibName;
}

-(NSBundle *)nibBundle {
   return _nibBundle;
}

-(NSUserInterfaceItemIdentifier)identifier {
   return _identifier;
}

-(void)setIdentifier:(NSUserInterfaceItemIdentifier)identifier {
   identifier=[identifier copy];
   [_identifier release];
   _identifier=identifier;
}

-(NSView *)view {
   if(_view==nil){
    [self loadView];
    if(_view!=nil)
     [self viewDidLoad];
   }
   return _view;
}

-(BOOL)isViewLoaded {
   return _view!=nil;
}

-(void)loadViewIfNeeded {
   [self view];
}

-(NSString *)title {
   return _title;
}

-representedObject {
   return _representedObject;
}

-(void)setRepresentedObject:object {
   object=[object retain];
   [_representedObject release];
   _representedObject=object;
}

-(void)setTitle:(NSString *)value {
   value=[value copy];
   [_title release];
   _title=value;
}

/* The controller comes between its view and the view's superview in the
   responder chain. */
-(void)setView:(NSView *)value {
   if(value==_view)
    return;
   if(_view!=nil && _NSViewControllerForView(_view)==self){
    objc_setAssociatedObject(_view,&controllerKey,nil,OBJC_ASSOCIATION_RETAIN);
    if([_view nextResponder]==self)
     [_view setNextResponder:[self nextResponder]];
   }
   value=[value retain];
   [_view release];
   _view=value;
   if(_view!=nil){
    objc_setAssociatedObject(_view,&controllerKey,[NSValue valueWithNonretainedObject:self],OBJC_ASSOCIATION_RETAIN);
    [self setNextResponder:[_view nextResponder]];
    [_view setNextResponder:self];
   }
}

/* The nib named nibName, or the class's name when it is nil. */
-(void)loadView {
   NSString *name=[self nibName];
   NSBundle *bundle=[self nibBundle];

   if(bundle==nil)
    bundle=[NSBundle bundleForClass:[self class]];
   if(name==nil){
    name=NSStringFromClass([self class]);
    if([bundle pathForResource:name ofType:@"nib"]==nil && [[NSBundle mainBundle] pathForResource:name ofType:@"nib"]==nil){
     [NSException raise:NSInternalInconsistencyException format:@"-[%@ %s] could not load the nib: it has no nibName and no nib of its class's name",[self class],sel_getName(_cmd)];
     return;
    }
   }
   if([bundle pathForResource:name ofType:@"nib"]==nil)
    bundle=[NSBundle mainBundle];

   NSString     *path=[bundle pathForResource:name ofType:@"nib"];
   NSDictionary *nameTable=[NSDictionary dictionaryWithObject:self forKey:NSNibOwner];

   if(path==nil){
    [NSException raise:NSInternalInconsistencyException format:@"-[%@ %s] could not load the nib '%@' in bundle %@",[self class],sel_getName(_cmd),name,bundle];
    return;
   }
   [bundle loadNibFile:path externalNameTable:nameTable withZone:NULL];
}

-(void)viewDidLoad {
}

-(void)viewWillAppear {
}

-(void)viewDidAppear {
}

-(void)viewWillDisappear {
}

-(void)viewDidDisappear {
}

-(void)updateViewConstraints {
}

-(void)viewWillLayout {
}

-(void)viewDidLayout {
}

-(NSSize)preferredContentSize {
   return _preferredContentSize;
}

/* The presenting popover, sheet or window follows the new size. */
-(void)setPreferredContentSize:(NSSize)size {
   _preferredContentSize=size;
   [_parentViewController preferredContentSizeDidChangeForViewController:self];
   [_presentingViewController preferredContentSizeDidChangeForViewController:self];
   if([_presentation isKindOfClass:[NSPopover class]])
    [(NSPopover *)_presentation setContentSize:size];
   else if([_presentation isKindOfClass:[NSWindow class]] && size.width>0 && size.height>0)
    [(NSWindow *)_presentation setContentSize:size];
}

-(NSSize)preferredMinimumSize {
   return ([self isViewLoaded])?[_view fittingSize]:NSZeroSize;
}

-(NSSize)preferredMaximumSize {
   return NSMakeSize(CGFLOAT_MAX,CGFLOAT_MAX);
}

-(void)preferredContentSizeDidChangeForViewController:(NSViewController *)viewController {
}

#pragma mark Children

-(NSArray *)childViewControllers {
   return (_childViewControllers!=nil)?[[_childViewControllers copy] autorelease]:[NSArray array];
}

-(void)setChildViewControllers:(NSArray *)controllers {
   while([_childViewControllers count]>0)
    [self removeChildViewControllerAtIndex:[_childViewControllers count]-1];
   for(NSViewController *child in controllers)
    [self addChildViewController:child];
}

-(void)addChildViewController:(NSViewController *)child {
   [self insertChildViewController:child atIndex:[_childViewControllers count]];
}

-(void)insertChildViewController:(NSViewController *)child atIndex:(NSInteger)index {
   if(child==nil)
    return;
   [child retain];
   [child removeFromParentViewController];
   if(_childViewControllers==nil)
    _childViewControllers=[[NSMutableArray alloc] init];
   [_childViewControllers insertObject:child atIndex:index];
   child->_parentViewController=self;
   [child release];
}

-(void)removeChildViewControllerAtIndex:(NSInteger)index {
   NSViewController *child=[_childViewControllers objectAtIndex:index];

   child->_parentViewController=nil;
   [_childViewControllers removeObjectAtIndex:index];
}

-(NSViewController *)parentViewController {
   return _parentViewController;
}

-(void)removeFromParentViewController {
   NSUInteger index;

   if(_parentViewController==nil)
    return;
   index=[_parentViewController->_childViewControllers indexOfObjectIdenticalTo:self];
   if(index!=NSNotFound)
    [_parentViewController removeChildViewControllerAtIndex:index];
}

-(void)transitionFromViewController:(NSViewController *)fromViewController toViewController:(NSViewController *)toViewController options:(NSViewControllerTransitionOptions)options completionHandler:(void (^)(void))completion {
   NSView *from=[fromViewController view];
   NSView *to=[toViewController view];
   NSView *superview=[from superview];

   if(superview!=nil && from!=to){
    [to setFrame:[from frame]];
    [to setAutoresizingMask:[from autoresizingMask]];
    [superview replaceSubview:from with:to];
   }
   if(completion!=NULL)
    completion();
}

#pragma mark Presentation

-(NSArray *)presentedViewControllers {
   return ([_presentedViewControllers count]>0)?[[_presentedViewControllers copy] autorelease]:nil;
}

-(NSViewController *)presentingViewController {
   return _presentingViewController;
}

-(void)_notePresented:(NSViewController *)viewController {
   if(_presentedViewControllers==nil)
    _presentedViewControllers=[[NSMutableArray alloc] init];
   [_presentedViewControllers addObject:viewController];
   viewController->_presentingViewController=self;
}

-(void)presentViewController:(NSViewController *)viewController animator:(id<NSViewControllerPresentationAnimator>)animator {
   [self _notePresented:viewController];
   [viewController->_presentation release];
   viewController->_presentation=[animator retain];
   [animator animatePresentationOfViewController:viewController fromViewController:self];
}

-(NSWindow *)_windowForPresentedViewController:(NSViewController *)viewController styleMask:(NSWindowStyleMask)styleMask {
   NSView  *view=[viewController view];
   NSSize   size=[viewController preferredContentSize];
   NSWindow *window;

   if(size.width<=0 || size.height<=0)
    size=[view frame].size;
   window=[[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,size.width,size.height) styleMask:styleMask backing:NSBackingStoreBuffered defer:NO] autorelease];
   [window setContentViewController:viewController];
   [window setReleasedWhenClosed:NO];
   if([viewController title]!=nil)
    [window setTitle:[viewController title]];
   return window;
}

-(void)presentViewControllerAsSheet:(NSViewController *)viewController {
   NSWindow *sheet=[self _windowForPresentedViewController:viewController styleMask:NSWindowStyleMaskTitled];

   [self _notePresented:viewController];
   [viewController->_presentation release];
   viewController->_presentation=[sheet retain];
   [[[self view] window] beginSheet:sheet completionHandler:NULL];
}

-(void)presentViewControllerAsModalWindow:(NSViewController *)viewController {
   NSWindow *window=[self _windowForPresentedViewController:viewController styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable];

   [self _notePresented:viewController];
   [viewController->_presentation release];
   viewController->_presentation=[window retain];
   [window center];
   [NSApp runModalForWindow:window];
}

-(void)presentViewController:(NSViewController *)viewController asPopoverRelativeToRect:(NSRect)positioningRect ofView:(NSView *)positioningView preferredEdge:(NSRectEdge)preferredEdge behavior:(NSPopoverBehavior)behavior {
   NSPopover *popover=[[[NSPopover alloc] init] autorelease];
   NSSize     size=[viewController preferredContentSize];

   [popover setContentViewController:viewController];
   if(size.width>0 && size.height>0)
    [popover setContentSize:size];
   [popover setBehavior:behavior];
   [self _notePresented:viewController];
   [viewController->_presentation release];
   viewController->_presentation=[popover retain];
   [popover showRelativeToRect:positioningRect ofView:positioningView preferredEdge:preferredEdge];
}

/* Closes the sheet, window or popover it was presented in. */
-(void)dismissViewController:(NSViewController *)viewController {
   id presentation=[[viewController->_presentation retain] autorelease];

   [viewController->_presentation release];
   viewController->_presentation=nil;
   if([presentation isKindOfClass:[NSPopover class]])
    [(NSPopover *)presentation close];
   else if([presentation isKindOfClass:[NSWindow class]]){
    NSWindow *window=presentation;

    if([window sheetParent]!=nil)
     [[window sheetParent] endSheet:window];
    else {
     if([NSApp modalWindow]==window)
      [NSApp stopModal];
     [window orderOut:nil];
    }
   }
   else if([presentation respondsToSelector:@selector(animateDismissalOfViewController:fromViewController:)])
    [presentation animateDismissalOfViewController:viewController fromViewController:self];
   viewController->_presentingViewController=nil;
   [_presentedViewControllers removeObjectIdenticalTo:viewController];
}

-(void)dismissController:(id)sender {
   if(_presentingViewController!=nil)
    [_presentingViewController dismissViewController:self];
}

-(void)discardEditing {
   [[[self view] window] endEditingFor:nil];
}

/* The window's field editor ends editing; YES when it could. */
-(BOOL)commitEditing {
   NSWindow *window=[_view window];

   return (window==nil)?YES:[window makeFirstResponder:window];
}

-(void)commitEditingWithDelegate:delegate didCommitSelector:(SEL)didCommitSelector contextInfo:(void *)contextInfo {
   BOOL committed=[self commitEditing];

   if(delegate!=nil && didCommitSelector!=NULL){
    void (*function)(id,SEL,id,BOOL,void *)=(void (*)(id,SEL,id,BOOL,void *))[delegate methodForSelector:didCommitSelector];

    function(delegate,didCommitSelector,self,committed,contextInfo);
   }
}

@end
