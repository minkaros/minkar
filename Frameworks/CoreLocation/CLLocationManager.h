/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>
#import <CoreLocation/CLLocation.h>
#import <CoreLocation/CLError.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(int, CLAuthorizationStatus) {
    kCLAuthorizationStatusNotDetermined = 0,
    kCLAuthorizationStatusRestricted = 1,
    kCLAuthorizationStatusDenied = 2,
    kCLAuthorizationStatusAuthorizedAlways = 3,
    kCLAuthorizationStatusAuthorized = kCLAuthorizationStatusAuthorizedAlways,
};

typedef NS_ENUM(NSInteger, CLAccuracyAuthorization) {
    CLAccuracyAuthorizationFullAccuracy = 0,
    CLAccuracyAuthorizationReducedAccuracy = 1,
};

@protocol CLLocationManagerDelegate;

@interface CLLocationManager : NSObject

+ (BOOL)locationServicesEnabled;
+ (CLAuthorizationStatus)authorizationStatus;
+ (BOOL)significantLocationChangeMonitoringAvailable;
+ (BOOL)headingAvailable;

@property (nullable, weak) id<CLLocationManagerDelegate> delegate;
@property CLLocationAccuracy desiredAccuracy;
@property CLLocationDistance distanceFilter;
@property (nullable, readonly, copy) CLLocation *location;
@property (readonly) CLAuthorizationStatus authorizationStatus;
@property (readonly) CLAccuracyAuthorization accuracyAuthorization;

- (void)requestWhenInUseAuthorization;
- (void)requestAlwaysAuthorization;
- (void)requestLocation;
- (void)startUpdatingLocation;
- (void)stopUpdatingLocation;
- (void)startMonitoringSignificantLocationChanges;
- (void)stopMonitoringSignificantLocationChanges;

@end

@protocol CLLocationManagerDelegate <NSObject>
@optional
- (void)locationManager:(CLLocationManager *)manager didUpdateLocations:(NSArray<CLLocation *> *)locations;
- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error;
- (void)locationManager:(CLLocationManager *)manager didChangeAuthorizationStatus:(CLAuthorizationStatus)status;
- (void)locationManagerDidChangeAuthorization:(CLLocationManager *)manager;
@end

NS_ASSUME_NONNULL_END
