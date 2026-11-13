/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreLocation/CoreLocation.h>
#include <math.h>

NSString *const kCLErrorDomain = @"kCLErrorDomain";

const CLLocationAccuracy kCLLocationAccuracyBestForNavigation = -2;
const CLLocationAccuracy kCLLocationAccuracyBest = -1;
const CLLocationAccuracy kCLLocationAccuracyNearestTenMeters = 10;
const CLLocationAccuracy kCLLocationAccuracyHundredMeters = 100;
const CLLocationAccuracy kCLLocationAccuracyKilometer = 1000;
const CLLocationAccuracy kCLLocationAccuracyThreeKilometers = 3000;
const CLLocationDistance kCLDistanceFilterNone = -1;

CLLocationCoordinate2D CLLocationCoordinate2DMake(CLLocationDegrees latitude, CLLocationDegrees longitude) {
    return (CLLocationCoordinate2D){ latitude, longitude };
}

BOOL CLLocationCoordinate2DIsValid(CLLocationCoordinate2D c) {
    return fabs(c.latitude) <= 90 && fabs(c.longitude) <= 180;
}

@implementation CLLocation

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithLatitude:(CLLocationDegrees)latitude longitude:(CLLocationDegrees)longitude {
    return [self initWithCoordinate:CLLocationCoordinate2DMake(latitude, longitude) altitude:0
                 horizontalAccuracy:0 verticalAccuracy:-1 timestamp:[NSDate date]];
}

- (instancetype)initWithCoordinate:(CLLocationCoordinate2D)coordinate altitude:(CLLocationDistance)altitude
                horizontalAccuracy:(CLLocationAccuracy)hAccuracy verticalAccuracy:(CLLocationAccuracy)vAccuracy
                         timestamp:(NSDate *)timestamp {
    if ((self = [super init]) != nil) {
        _coordinate = coordinate;
        _altitude = altitude;
        _horizontalAccuracy = hAccuracy;
        _verticalAccuracy = vAccuracy;
        _course = -1;
        _speed = -1;
        _timestamp = [timestamp copy];
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeDouble:_coordinate.latitude forKey:@"latitude"];
    [coder encodeDouble:_coordinate.longitude forKey:@"longitude"];
    [coder encodeDouble:_altitude forKey:@"altitude"];
    [coder encodeDouble:_horizontalAccuracy forKey:@"horizontalAccuracy"];
    [coder encodeDouble:_verticalAccuracy forKey:@"verticalAccuracy"];
    [coder encodeObject:_timestamp forKey:@"timestamp"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithCoordinate:CLLocationCoordinate2DMake([coder decodeDoubleForKey:@"latitude"],
                                                               [coder decodeDoubleForKey:@"longitude"])
                           altitude:[coder decodeDoubleForKey:@"altitude"]
                 horizontalAccuracy:[coder decodeDoubleForKey:@"horizontalAccuracy"]
                   verticalAccuracy:[coder decodeDoubleForKey:@"verticalAccuracy"]
                          timestamp:[coder decodeObjectOfClass:[NSDate class] forKey:@"timestamp"]];
}

/* Great-circle distance on a sphere of the Earth's mean radius. */
- (CLLocationDistance)distanceFromLocation:(CLLocation *)other {
    const double radius = 6371008.8, rad = M_PI / 180;
    double lat1 = _coordinate.latitude * rad, lat2 = other.coordinate.latitude * rad;
    double dlat = lat2 - lat1, dlon = (other.coordinate.longitude - _coordinate.longitude) * rad;
    double a = sin(dlat / 2) * sin(dlat / 2) + cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
    return 2 * radius * atan2(sqrt(a), sqrt(1 - a));
}

@end

@implementation CLLocationManager

/* No location service runs on Minkar. */
+ (BOOL)locationServicesEnabled {
    return NO;
}

+ (CLAuthorizationStatus)authorizationStatus {
    return kCLAuthorizationStatusRestricted;
}

+ (BOOL)significantLocationChangeMonitoringAvailable {
    return NO;
}

+ (BOOL)headingAvailable {
    return NO;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _desiredAccuracy = kCLLocationAccuracyBest;
        _distanceFilter = kCLDistanceFilterNone;
    }
    return self;
}

- (CLAuthorizationStatus)authorizationStatus {
    return [CLLocationManager authorizationStatus];
}

- (CLAccuracyAuthorization)accuracyAuthorization {
    return CLAccuracyAuthorizationReducedAccuracy;
}

- (CLLocation *)location {
    return nil;
}

- (void)_tellAuthorization {
    id<CLLocationManagerDelegate> delegate = _delegate;
    if ([delegate respondsToSelector:@selector(locationManagerDidChangeAuthorization:)])
        [delegate locationManagerDidChangeAuthorization:self];
    else if ([delegate respondsToSelector:@selector(locationManager:didChangeAuthorizationStatus:)])
        [delegate locationManager:self didChangeAuthorizationStatus:[self authorizationStatus]];
}

- (void)requestWhenInUseAuthorization {
    dispatch_async(dispatch_get_main_queue(), ^{ [self _tellAuthorization]; });
}

- (void)requestAlwaysAuthorization {
    [self requestWhenInUseAuthorization];
}

- (void)_fail {
    dispatch_async(dispatch_get_main_queue(), ^{
        id<CLLocationManagerDelegate> delegate = self.delegate;
        if ([delegate respondsToSelector:@selector(locationManager:didFailWithError:)])
            [delegate locationManager:self didFailWithError:[NSError errorWithDomain:kCLErrorDomain
                                                                                code:kCLErrorDenied userInfo:nil]];
    });
}

- (void)requestLocation {
    [self _fail];
}

- (void)startUpdatingLocation {
    [self _fail];
}

- (void)stopUpdatingLocation {
}

- (void)startMonitoringSignificantLocationChanges {
    [self _fail];
}

- (void)stopMonitoringSignificantLocationChanges {
}

@end
