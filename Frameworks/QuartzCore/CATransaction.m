#import <QuartzCore/CATransaction.h>
#import <dispatch/dispatch.h>
#import <QuartzCore/CALayerContext.h>
#import <Foundation/NSString.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSThread-Private.h>
#import "CATransactionGroup.h"

NSString * const kCATransactionAnimationDuration=@"kCATransactionAnimationDuration";
NSString * const kCATransactionDisableActions=@"kCATransactionDisableActions";
NSString * const kCATransactionAnimationTimingFunction=@"kCATransactionAnimationTimingFunction";
NSString * const kCATransactionCompletionBlock=@"kCATransactionCompletionBlock";

@implementation CATransaction

static NSMutableArray *transactionStack(){
   NSMutableDictionary *shared=[NSCurrentThread() sharedDictionary];
   id                   stack=[shared objectForKey:@"CATransactionStack"];
   
   if(stack==nil){
    stack=[NSMutableArray array];
    [shared setObject:stack forKey:@"CATransactionStack"];
   }
      
   return stack;
}

static CATransactionGroup *currentTransactionGroup(){
   id stack=transactionStack();
   
   CATransactionGroup *result=[stack lastObject];

   return result;
}

static CATransactionGroup *createImplicitTransactionGroupIfNeeded(){
   CATransactionGroup *check=currentTransactionGroup();
   
   if(check==nil){
    check=[[CATransactionGroup alloc] init];
   
    [transactionStack() addObject:check];
    [[NSRunLoop currentRunLoop] performSelector:@selector(commit) target:[CATransaction class] argument:nil order:0 modes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];
   }
   
   return check;
}


+(BOOL)disableActions {
   return [[self valueForKey:kCATransactionDisableActions] boolValue];
}

+(CFTimeInterval)animationDuration {
   return [[self valueForKey:kCATransactionAnimationDuration] doubleValue];
}

+(CAMediaTimingFunction *)animationTimingFunction {
   return [self valueForKey:kCATransactionAnimationTimingFunction];
}

/* Minkar: the completion block, kept with the transaction; changes take
   effect at once in Minkar, so it runs when the transaction is
   committed, or (without one begun) once the run loop turns. */
static NSString *const completionBlockKey=@"completionBlock";

+(void (^)(void))completionBlock {
   return [self valueForKey:completionBlockKey];
}

+(void)setCompletionBlock:(void (^)(void))block {
   void (^copy)(void)=[[block copy] autorelease];

   if([transactionStack() count]==0){
    if(copy)
     dispatch_async(dispatch_get_main_queue(),copy);
    return;
   }
   [self setValue:copy forKey:completionBlockKey];
}

+valueForKey:(NSString *)key {
   CATransactionGroup *group=createImplicitTransactionGroupIfNeeded();
   
   return [group valueForKey:key];
}

+(void)setAnimationDuration:(CFTimeInterval)value {
   [self setValue:[NSNumber numberWithDouble:value] forKey:kCATransactionAnimationDuration];
}

+(void)setAnimationTimingFunction:(CAMediaTimingFunction *)value {
   [self setValue:value forKey:kCATransactionAnimationTimingFunction];
}

+(void)setDisableActions:(BOOL)value {
   [self setValue:[NSNumber numberWithBool:value] forKey:kCATransactionDisableActions];
}

+(void)setValue:value forKey:(NSString *)key {
   CATransactionGroup *group=createImplicitTransactionGroupIfNeeded();
   
   [group setValue:value forKey:key];
}

+(void)begin {
   CATransactionGroup *group=[[CATransactionGroup alloc] init];
   
   [transactionStack() addObject:group];
}

+(void)commit {
   CATransactionGroup *group=[[transactionStack() lastObject] retain];
   void (^block)(void)=[group valueForKey:completionBlockKey];

   [transactionStack() removeLastObject];
   if(block)
    dispatch_async(dispatch_get_main_queue(),block);
   [group release];
}

+(void)flush {
}

+(void)lock {
}

+(void)unlock {
}

@end
