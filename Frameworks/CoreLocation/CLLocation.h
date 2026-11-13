/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef double CLLocationDegrees;
typedef double CLLocationAccuracy;
typedef double CLLocationSpeed;
typedef double CLLocationDirection;
typedef double CLLocationDistance;

typedef struct {
    CLLocationDegrees latitude;
    CLLocationDegrees longitude;
} CLLocationCoordinate2D;

/* Accuracies, in meters; negative values are requests for the best. */
FOUNDATION_EXPORT const CLLocationAccuracy kCLLocationAccuracyBestForNavigation;
FOUNDATION_EXPORT const CLLocationAccuracy kCLLocationAccuracyBest;
FOUNDATION_EXPORT const CLLocationAccuracy kCLLocationAccuracyNearestTenMeters;
FOUNDATION_EXPORT const CLLocationAccuracy kCLLocationAccuracyHundredMeters;
FOUNDATION_EXPORT const CLLocationAccuracy kCLLocationAccuracyKilometer;
FOUNDATION_EXPORT const CLLocationAccuracy kCLLocationAccuracyThreeKilometers;
FOUNDATION_EXPORT const CLLocationDistance kCLDistanceFilterNone;

FOUNDATION_EXPORT CLLocationCoordinate2D CLLocationCoordinate2DMake(CLLocationDegrees latitude, CLLocationDegrees longitude);
FOUNDATION_EXPORT BOOL CLLocationCoordinate2DIsValid(CLLocationCoordinate2D coordinate);

@interface CLLocation : NSObject <NSCopying, NSSecureCoding>
- (instancetype)initWithLatitude:(CLLocationDegrees)latitude longitude:(CLLocationDegrees)longitude;
- (instancetype)initWithCoordinate:(CLLocationCoordinate2D)coordinate altitude:(CLLocationDistance)altitude
                horizontalAccuracy:(CLLocationAccuracy)hAccuracy verticalAccuracy:(CLLocationAccuracy)vAccuracy
                         timestamp:(NSDate *)timestamp;
@property (readonly) CLLocationCoordinate2D coordinate;
@property (readonly) CLLocationDistance altitude;
@property (readonly) CLLocationAccuracy horizontalAccuracy;
@property (readonly) CLLocationAccuracy verticalAccuracy;
@property (readonly) CLLocationDirection course;
@property (readonly) CLLocationSpeed speed;
@property (readonly, copy) NSDate *timestamp;
- (CLLocationDistance)distanceFromLocation:(CLLocation *)location;
@end

NS_ASSUME_NONNULL_END
