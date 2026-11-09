/* NSViewController: a view and what it shows, loaded from a nib or made
   in code; with child controllers and presentation (API 19). */

#import <AppKit/NSResponder.h>
#import <AppKit/NSView.h>
#import <AppKit/NSPopover.h>

@class NSBundle, NSArray, NSWindow, NSStoryboard;

typedef NS_OPTIONS(NSUInteger, NSViewControllerTransitionOptions) {
    NSViewControllerTransitionNone = 0x0,
    NSViewControllerTransitionCrossfade = 0x1,
    NSViewControllerTransitionSlideUp = 0x10,
    NSViewControllerTransitionSlideDown = 0x20,
    NSViewControllerTransitionSlideLeft = 0x40,
    NSViewControllerTransitionSlideRight = 0x80,
    NSViewControllerTransitionSlideForward = 0x140,
    NSViewControllerTransitionSlideBackward = 0x180,
    NSViewControllerTransitionAllowUserInteraction = 0x1000,
};

@protocol NSViewControllerPresentationAnimator;

@interface NSViewController : NSResponder <NSUserInterfaceItemIdentification> {
    NSString *_nibName;
    NSBundle *_nibBundle;
    id _representedObject;
    NSString *_title;
    NSView *_view;
    NSMutableArray *_childViewControllers;
    NSViewController *_parentViewController;
    NSMutableArray *_presentedViewControllers;
    NSViewController *_presentingViewController;
    NSSize _preferredContentSize;
    NSString *_identifier;
    id _presentation;
}

- (instancetype)initWithNibName:(NSString *)name bundle:(NSBundle *)bundle;

@property (readonly, copy) NSString *nibName;
@property (readonly, strong) NSBundle *nibBundle;
@property (strong) id representedObject;
@property (copy) NSString *title;
@property (strong) NSView *view;
@property (readonly, getter=isViewLoaded) BOOL viewLoaded;
@property (copy) NSUserInterfaceItemIdentifier identifier;

/* Loads the nib (nibName, or the class's name when nil) or raises;
   viewDidLoad follows. */
- (void)loadView;
- (void)loadViewIfNeeded;
- (void)viewDidLoad;
- (void)viewWillAppear;
- (void)viewDidAppear;
- (void)viewWillDisappear;
- (void)viewDidDisappear;
- (void)updateViewConstraints;
- (void)viewWillLayout;
- (void)viewDidLayout;

/* The size the controller would like its view to be in a popover, a
   sheet or a split view. */
@property NSSize preferredContentSize;
@property (readonly) NSSize preferredMinimumSize;
@property (readonly) NSSize preferredMaximumSize;
- (void)preferredContentSizeDidChangeForViewController:(NSViewController *)viewController;

@property (copy) NSArray<__kindof NSViewController *> *childViewControllers;
@property (readonly, assign) NSViewController *parentViewController;
- (void)addChildViewController:(NSViewController *)childViewController;
- (void)insertChildViewController:(NSViewController *)childViewController atIndex:(NSInteger)index;
- (void)removeChildViewControllerAtIndex:(NSInteger)index;
- (void)removeFromParentViewController;
/* Puts toViewController's view where fromViewController's was. */
- (void)transitionFromViewController:(NSViewController *)fromViewController toViewController:(NSViewController *)toViewController options:(NSViewControllerTransitionOptions)options completionHandler:(void (^)(void))completion;

- (void)presentViewController:(NSViewController *)viewController animator:(id<NSViewControllerPresentationAnimator>)animator;
- (void)presentViewControllerAsSheet:(NSViewController *)viewController;
- (void)presentViewControllerAsModalWindow:(NSViewController *)viewController;
- (void)presentViewController:(NSViewController *)viewController asPopoverRelativeToRect:(NSRect)positioningRect ofView:(NSView *)positioningView preferredEdge:(NSRectEdge)preferredEdge behavior:(NSPopoverBehavior)behavior;
- (void)dismissViewController:(NSViewController *)viewController;
- (IBAction)dismissController:(id)sender;
@property (readonly, copy) NSArray<__kindof NSViewController *> *presentedViewControllers;
@property (readonly, assign) NSViewController *presentingViewController;

- (void)discardEditing;
- (BOOL)commitEditing;
- (void)commitEditingWithDelegate:delegate didCommitSelector:(SEL)didCommitSelector contextInfo:(void *)contextInfo;

@end

@protocol NSViewControllerPresentationAnimator <NSObject>
- (void)animatePresentationOfViewController:(NSViewController *)viewController fromViewController:(NSViewController *)fromViewController;
- (void)animateDismissalOfViewController:(NSViewController *)viewController fromViewController:(NSViewController *)fromViewController;
@end

/* The controller whose view this is, for AppKit. */
APPKIT_EXPORT NSViewController *_NSViewControllerForView(NSView *view);
