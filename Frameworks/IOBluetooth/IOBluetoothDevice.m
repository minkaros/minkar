/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <IOBluetooth/IOBluetooth.h>

@implementation IOBluetoothDevice {
    BluetoothDeviceAddress _address;
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

/* No Bluetooth controller is driven: no devices. */
+ (NSArray<IOBluetoothDevice *> *)pairedDevices {
    return @[];
}

+ (NSArray<IOBluetoothDevice *> *)recentDevices:(unsigned long)numDevices {
    return @[];
}

+ (instancetype)deviceWithAddressString:(NSString *)address {
    return nil;
}

- (NSString *)name {
    return nil;
}

- (NSString *)addressString {
    return [NSString stringWithFormat:@"%02x-%02x-%02x-%02x-%02x-%02x", _address.data[0], _address.data[1],
        _address.data[2], _address.data[3], _address.data[4], _address.data[5]];
}

- (NSString *)nameOrAddress {
    return self.name ?: self.addressString;
}

- (BluetoothClassOfDevice)classOfDevice {
    return 0;
}

- (BOOL)isConnected {
    return NO;
}

- (BOOL)isPaired {
    return NO;
}

- (const BluetoothDeviceAddress *)getAddress {
    return &_address;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.addressString forKey:@"address"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [super init];
}

@end
