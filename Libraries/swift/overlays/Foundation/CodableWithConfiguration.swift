// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// API 21 (12.0): coding with a configuration, for types that need more
// than the coder to encode or decode themselves. A configuration is
// passed as a value or by a type that provides it. Keyed and unkeyed
// containers code a value through a nested coder; a single value
// container encodes it through a wrapper.

@_exported import Foundation

@available(macOS 12.0, *)
public protocol EncodingConfigurationProviding {
    associatedtype EncodingConfiguration
    static var encodingConfiguration: EncodingConfiguration { get }
}

@available(macOS 12.0, *)
public protocol EncodableWithConfiguration {
    associatedtype EncodingConfiguration
    func encode(to encoder: Encoder, configuration: EncodingConfiguration) throws
}

@available(macOS 12.0, *)
public protocol DecodingConfigurationProviding {
    associatedtype DecodingConfiguration
    static var decodingConfiguration: DecodingConfiguration { get }
}

@available(macOS 12.0, *)
public protocol DecodableWithConfiguration {
    associatedtype DecodingConfiguration
    init(from decoder: Decoder, configuration: DecodingConfiguration) throws
}

@available(macOS 12.0, *)
public typealias CodableWithConfiguration = EncodableWithConfiguration & DecodableWithConfiguration

// A value and its configuration, as an Encodable.
@available(macOS 12.0, *)
private struct ConfiguredEncodable<T: EncodableWithConfiguration>: Encodable {
    let value: T
    let configuration: T.EncodingConfiguration

    func encode(to encoder: Encoder) throws {
        try value.encode(to: encoder, configuration: configuration)
    }
}

@available(macOS 12.0, *)
extension KeyedEncodingContainer {
    public mutating func encode<T: EncodableWithConfiguration>(
        _ t: T, forKey key: Key, configuration: T.EncodingConfiguration) throws {
        try t.encode(to: superEncoder(forKey: key), configuration: configuration)
    }

    public mutating func encodeIfPresent<T: EncodableWithConfiguration>(
        _ t: T?, forKey key: Key, configuration: T.EncodingConfiguration) throws {
        guard let t = t else { return }
        try encode(t, forKey: key, configuration: configuration)
    }

    public mutating func encode<T, C>(_ t: T, forKey key: Key, configuration: C.Type) throws
        where T: EncodableWithConfiguration, C: EncodingConfigurationProviding,
              T.EncodingConfiguration == C.EncodingConfiguration {
        try encode(t, forKey: key, configuration: C.encodingConfiguration)
    }

    public mutating func encodeIfPresent<T, C>(_ t: T?, forKey key: Key, configuration: C.Type) throws
        where T: EncodableWithConfiguration, C: EncodingConfigurationProviding,
              T.EncodingConfiguration == C.EncodingConfiguration {
        try encodeIfPresent(t, forKey: key, configuration: C.encodingConfiguration)
    }
}

@available(macOS 12.0, *)
extension KeyedDecodingContainer {
    public func decode<T: DecodableWithConfiguration>(
        _ type: T.Type, forKey key: Key, configuration: T.DecodingConfiguration) throws -> T {
        return try T(from: superDecoder(forKey: key), configuration: configuration)
    }

    public func decodeIfPresent<T: DecodableWithConfiguration>(
        _ type: T.Type, forKey key: Key, configuration: T.DecodingConfiguration) throws -> T? {
        guard contains(key), try !decodeNil(forKey: key) else { return nil }
        return try decode(type, forKey: key, configuration: configuration)
    }

    public func decode<T, C>(_ type: T.Type, forKey key: Key, configuration: C.Type) throws -> T
        where T: DecodableWithConfiguration, C: DecodingConfigurationProviding,
              T.DecodingConfiguration == C.DecodingConfiguration {
        return try decode(type, forKey: key, configuration: C.decodingConfiguration)
    }

    public func decodeIfPresent<T, C>(_ type: T.Type, forKey key: Key, configuration: C.Type) throws -> T?
        where T: DecodableWithConfiguration, C: DecodingConfigurationProviding,
              T.DecodingConfiguration == C.DecodingConfiguration {
        return try decodeIfPresent(type, forKey: key, configuration: C.decodingConfiguration)
    }
}

@available(macOS 12.0, *)
extension UnkeyedEncodingContainer {
    public mutating func encode<T: EncodableWithConfiguration>(
        _ t: T, configuration: T.EncodingConfiguration) throws {
        try t.encode(to: superEncoder(), configuration: configuration)
    }

    public mutating func encode<T, C>(_ t: T, configuration: C.Type) throws
        where T: EncodableWithConfiguration, C: EncodingConfigurationProviding,
              T.EncodingConfiguration == C.EncodingConfiguration {
        try encode(t, configuration: C.encodingConfiguration)
    }
}

@available(macOS 12.0, *)
extension UnkeyedDecodingContainer {
    public mutating func decode<T: DecodableWithConfiguration>(
        _ type: T.Type, configuration: T.DecodingConfiguration) throws -> T {
        return try T(from: superDecoder(), configuration: configuration)
    }

    public mutating func decodeIfPresent<T: DecodableWithConfiguration>(
        _ type: T.Type, configuration: T.DecodingConfiguration) throws -> T? {
        if try decodeNil() { return nil }
        return try decode(type, configuration: configuration)
    }

    public mutating func decode<T, C>(_ type: T.Type, configuration: C.Type) throws -> T
        where T: DecodableWithConfiguration, C: DecodingConfigurationProviding,
              T.DecodingConfiguration == C.DecodingConfiguration {
        return try decode(type, configuration: C.decodingConfiguration)
    }
}

@available(macOS 12.0, *)
extension SingleValueEncodingContainer {
    public mutating func encode<T: EncodableWithConfiguration>(
        _ t: T, configuration: T.EncodingConfiguration) throws {
        try encode(ConfiguredEncodable(value: t, configuration: configuration))
    }

    public mutating func encode<T, C>(_ t: T, configuration: C.Type) throws
        where T: EncodableWithConfiguration, C: EncodingConfigurationProviding,
              T.EncodingConfiguration == C.EncodingConfiguration {
        try encode(t, configuration: C.encodingConfiguration)
    }
}

// A property coded with the configuration its provider gives.
@available(macOS 12.0, *)
@propertyWrapper
public struct CodableConfiguration<T, ConfigurationProvider: EncodingConfigurationProviding & DecodingConfigurationProviding>: Codable
    where T: EncodableWithConfiguration & DecodableWithConfiguration,
          T.EncodingConfiguration == ConfigurationProvider.EncodingConfiguration,
          T.DecodingConfiguration == ConfigurationProvider.DecodingConfiguration {
    public var wrappedValue: T

    public init(wrappedValue: T) {
        self.wrappedValue = wrappedValue
    }

    public init(wrappedValue: T, from configurationProvider: ConfigurationProvider.Type) {
        self.wrappedValue = wrappedValue
    }

    public init(from decoder: Decoder) throws {
        wrappedValue = try T(from: decoder, configuration: ConfigurationProvider.decodingConfiguration)
    }

    public func encode(to encoder: Encoder) throws {
        try wrappedValue.encode(to: encoder, configuration: ConfigurationProvider.encodingConfiguration)
    }
}
