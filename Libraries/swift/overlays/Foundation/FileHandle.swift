// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// FileHandle's throwing API (API 19) where Swift's names cannot come
// from the header: the offset methods return it, and write(contentsOf:)
// takes any DataProtocol.

@_exported import Foundation

extension FileHandle {
    @available(macOS 10.15, *)
    public func offset() throws -> UInt64 {
        var offset: UInt64 = 0
        try __getOffset(&offset)
        return offset
    }

    @available(macOS 10.15, *)
    @discardableResult
    public func seekToEnd() throws -> UInt64 {
        var offset: UInt64 = 0
        try __seek(toEndReturningOffset: &offset)
        return offset
    }

    @available(macOS 10.15, *)
    public func write<T: DataProtocol>(contentsOf data: T) throws {
        for region in data.regions {
            try region.withUnsafeBytes { bytes in
                if let base = bytes.baseAddress, bytes.count > 0 {
                    try __write(contentsOf: Data(bytesNoCopy: UnsafeMutableRawPointer(mutating: base),
                                                 count: bytes.count, deallocator: .none))
                }
            }
        }
    }
}
