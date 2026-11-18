#import <Foundation/NSTextCheckingResult.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <Foundation/NSObjCRuntime.h>

NSString *const NSTextCheckingNameKey=@"Name";
NSString *const NSTextCheckingJobTitleKey=@"JobTitle";
NSString *const NSTextCheckingOrganizationKey=@"Organization";
NSString *const NSTextCheckingStreetKey=@"Street";
NSString *const NSTextCheckingCityKey=@"City";
NSString *const NSTextCheckingStateKey=@"State";
NSString *const NSTextCheckingZIPKey=@"ZIP";
NSString *const NSTextCheckingCountryKey=@"Country";
NSString *const NSTextCheckingPhoneKey=@"Phone";
NSString *const NSTextCheckingAirlineKey=@"Airline";
NSString *const NSTextCheckingFlightKey=@"Flight";

@implementation NSTextCheckingResult

-initWithResultType:(NSTextCheckingType)resultType range:(NSRange)range properties:(NSDictionary *)properties {
   _resultType=resultType;
   _range=range;
   _properties=[properties copy];
   return self;
}

-initWithResultType:(NSTextCheckingType)resultType range:(NSRange)range property:property name:(NSString *)name {
   NSDictionary *properties=[NSDictionary dictionaryWithObject:property forKey:name];

   return [self initWithResultType:resultType range:range properties:properties];
}

-(void)dealloc {
   [_properties release];
   [super dealloc];
}

/* The kinds of result and what each holds, kept in its properties. */
static NSString *const NSTCRComponents=@"components";
static NSString *const NSTCRReplacement=@"replacement";
static NSString *const NSTCRAlternatives=@"alternatives";
static NSString *const NSTCRDate=@"date";
static NSString *const NSTCRTimeZone=@"timeZone";
static NSString *const NSTCRDuration=@"duration";
static NSString *const NSTCRDetails=@"details";
static NSString *const NSTCRURL=@"URL";
static NSString *const NSTCROrthography=@"orthography";
static NSString *const NSTCRPhoneNumber=@"phoneNumber";

static NSTextCheckingResult *resultWith(Class cls,NSTextCheckingType type,NSRange range,NSDictionary *properties) {
   return [[[cls alloc] initWithResultType:type range:range properties:properties] autorelease];
}

static NSDictionary *one(id value,NSString *key) {
   return (value!=nil)?[NSDictionary dictionaryWithObject:value forKey:key]:nil;
}

+(NSTextCheckingResult *)addressCheckingResultWithRange:(NSRange)range components:(NSDictionary *)components {
   return resultWith(self,NSTextCheckingTypeAddress,range,one(components,NSTCRComponents));
}

+(NSTextCheckingResult *)correctionCheckingResultWithRange:(NSRange)range replacementString:(NSString *)replacement {
   return resultWith(self,NSTextCheckingTypeCorrection,range,one(replacement,NSTCRReplacement));
}

+(NSTextCheckingResult *)correctionCheckingResultWithRange:(NSRange)range replacementString:(NSString *)replacement alternativeStrings:(NSArray *)alternatives {
   NSMutableDictionary *p=[NSMutableDictionary dictionary];

   if(replacement!=nil) [p setObject:replacement forKey:NSTCRReplacement];
   if(alternatives!=nil) [p setObject:alternatives forKey:NSTCRAlternatives];
   return resultWith(self,NSTextCheckingTypeCorrection,range,p);
}

+(NSTextCheckingResult *)dashCheckingResultWithRange:(NSRange)range replacementString:(NSString *)replacement {
   return resultWith(self,NSTextCheckingTypeDash,range,one(replacement,NSTCRReplacement));
}

+(NSTextCheckingResult *)dateCheckingResultWithRange:(NSRange)range date:(NSDate *)date {
   return resultWith(self,NSTextCheckingTypeDate,range,one(date,NSTCRDate));
}

+(NSTextCheckingResult *)dateCheckingResultWithRange:(NSRange)range date:(NSDate *)date timeZone:(NSTimeZone *)timeZone duration:(NSTimeInterval)duration {
   NSMutableDictionary *p=[NSMutableDictionary dictionary];

   if(date!=nil) [p setObject:date forKey:NSTCRDate];
   if(timeZone!=nil) [p setObject:timeZone forKey:NSTCRTimeZone];
   [p setObject:[NSNumber numberWithDouble:duration] forKey:NSTCRDuration];
   return resultWith(self,NSTextCheckingTypeDate,range,p);
}

+(NSTextCheckingResult *)grammarCheckingResultWithRange:(NSRange)range details:(NSArray *)details {
   return resultWith(self,NSTextCheckingTypeGrammar,range,one(details,NSTCRDetails));
}

+(NSTextCheckingResult *)linkCheckingResultWithRange:(NSRange)range URL:(NSURL *)url {
   return resultWith(self,NSTextCheckingTypeLink,range,one(url,NSTCRURL));
}

+(NSTextCheckingResult *)orthographyCheckingResultWithRange:(NSRange)range orthography:(NSOrthography *)orthography {
   return resultWith(self,NSTextCheckingTypeOrthography,range,one(orthography,NSTCROrthography));
}

+(NSTextCheckingResult *)quoteCheckingResultWithRange:(NSRange)range replacementString:(NSString *)replacement {
   return resultWith(self,NSTextCheckingTypeQuote,range,one(replacement,NSTCRReplacement));
}

+(NSTextCheckingResult *)replacementCheckingResultWithRange:(NSRange)range replacementString:(NSString *)replacement {
   return resultWith(self,NSTextCheckingTypeReplacement,range,one(replacement,NSTCRReplacement));
}

+(NSTextCheckingResult *)spellCheckingResultWithRange:(NSRange)range {
   return [[[self alloc] initWithResultType:NSTextCheckingTypeSpelling range:range properties:nil] autorelease];
}

+(NSTextCheckingResult *)phoneNumberCheckingResultWithRange:(NSRange)range phoneNumber:(NSString *)phoneNumber {
   return resultWith(self,NSTextCheckingTypePhoneNumber,range,one(phoneNumber,NSTCRPhoneNumber));
}

+(NSTextCheckingResult *)transitInformationCheckingResultWithRange:(NSRange)range components:(NSDictionary *)components {
   return resultWith(self,NSTextCheckingTypeTransitInformation,range,one(components,NSTCRComponents));
}

-(NSDictionary *)addressComponents {
   return (_resultType==NSTextCheckingTypeAddress)?[_properties objectForKey:NSTCRComponents]:nil;
}

-(NSDictionary *)components {
   return [_properties objectForKey:NSTCRComponents];
}

-(NSDate *)date {
   return [_properties objectForKey:NSTCRDate];
}

-(NSTimeInterval)duration {
   return [[_properties objectForKey:NSTCRDuration] doubleValue];
}

-(NSArray *)grammarDetails {
   return [_properties objectForKey:NSTCRDetails];
}

-(NSOrthography *)orthography {
   return [_properties objectForKey:NSTCROrthography];
}

-(NSRange)range {
   return _range;
}

-(NSString *)replacementString {
   return [_properties objectForKey:NSTCRReplacement];
}

-(NSArray *)alternativeStrings {
   return [_properties objectForKey:NSTCRAlternatives];
}

-(NSString *)phoneNumber {
   return [_properties objectForKey:NSTCRPhoneNumber];
}

-(NSTextCheckingType)resultType {
   return _resultType;
}

-(NSTimeZone *)timeZone {
   return [_properties objectForKey:NSTCRTimeZone];
}

-(NSURL *)URL {
   return [_properties objectForKey:NSTCRURL];
}

/* A result of one range (the regular expression's have their own). */
-(NSUInteger)numberOfRanges {
   return 1;
}

-(NSRange)rangeAtIndex:(NSUInteger)index {
   if(index!=0)
    [NSException raise:NSRangeException format:@"-[%@ %@]: index %lu beyond bounds [0 .. 0]",[self class],NSStringFromSelector(_cmd),(unsigned long)index];
   return _range;
}

-(NSRegularExpression *)regularExpression {
   return nil;
}

-(NSTextCheckingResult *)resultByAdjustingRangesWithOffset:(NSInteger)offset {
   NSRange range=_range;

   if(range.location!=NSNotFound)
    range.location+=offset;
   return resultWith([self class],_resultType,range,_properties);
}

@end
