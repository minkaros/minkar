/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>

typedef struct BluetoothDeviceAddress {
    uint8_t data[6];
} BluetoothDeviceAddress;

typedef uint32_t BluetoothClassOfDevice;

@interface IOBluetoothDevice : NSObject <NSSecureCoding>
+ (NSArray<IOBluetoothDevice *> *)pairedDevices;
+ (NSArray<IOBluetoothDevice *> *)recentDevices:(unsigned long)numDevices;
+ (instancetype)deviceWithAddressString:(NSString *)address;
@property (readonly, copy) NSString *name;
@property (readonly, copy) NSString *nameOrAddress;
@property (readonly, copy) NSString *addressString;
@property (readonly) BluetoothClassOfDevice classOfDevice;
- (BOOL)isConnected;
- (BOOL)isPaired;
- (const BluetoothDeviceAddress *)getAddress;
@end
