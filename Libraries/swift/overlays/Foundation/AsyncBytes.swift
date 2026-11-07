// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// API 21 (12.0): a file handle's bytes as an asynchronous sequence, and
// the lines of any asynchronous sequence of UTF-8 bytes. Reads go to a
// global queue, a buffer at a time.

@_exported import Foundation
import Dispatch

@available(macOS 12.0, *)
extension FileHandle {
    public struct AsyncBytes: AsyncSequence {
        public typealias Element = UInt8

        let handle: FileHandle

        public struct Iterator: AsyncIteratorProtocol {
            let handle: FileHandle
            var buffer: [UInt8] = []
            var index = 0
            var ended = false

            public mutating func next() async throws -> UInt8? {
                if index == buffer.count {
                    if ended { return nil }
                    let handle = self.handle
                    let data: Data = try await withCheckedThrowingContinuation { done in
                        DispatchQueue.global().async {
                            do {
                                done.resume(returning: try handle.read(upToCount: 65536) ?? Data())
                            } catch {
                                done.resume(throwing: error)
                            }
                        }
                    }
                    if data.isEmpty {
                        ended = true
                        return nil
                    }
                    buffer = [UInt8](data)
                    index = 0
                }
                defer { index += 1 }
                return buffer[index]
            }
        }

        public func makeAsyncIterator() -> Iterator {
            return Iterator(handle: handle)
        }
    }

    public var bytes: AsyncBytes {
        return AsyncBytes(handle: self)
    }
}

// Lines end at LF, CR, CR LF, NEL, LS or PS; the terminators are not
// part of them, and a last line without one is still a line.
@available(macOS 12.0, *)
public struct AsyncLineSequence<Base: AsyncSequence>: AsyncSequence where Base.Element == UInt8 {
    public typealias Element = String

    var base: Base

    public struct AsyncIterator: AsyncIteratorProtocol {
        var base: Base.AsyncIterator
        var line: [UInt8] = []
        var pending: UInt8?
        var ended = false

        mutating func nextByte() async throws -> UInt8? {
            if let byte = pending {
                pending = nil
                return byte
            }
            return try await base.next()
        }

        public mutating func next() async throws -> String? {
            if ended { return nil }
            line.removeAll(keepingCapacity: true)
            while true {
                guard let byte = try await nextByte() else {
                    ended = true
                    return line.isEmpty ? nil : String(decoding: line, as: UTF8.self)
                }
                switch byte {
                case 0x0A:
                    return String(decoding: line, as: UTF8.self)
                case 0x0D:
                    if let after = try await nextByte(), after != 0x0A { pending = after }
                    return String(decoding: line, as: UTF8.self)
                default:
                    line.append(byte)
                    // NEL (C2 85), LS (E2 80 A8) and PS (E2 80 A9).
                    let n = line.count
                    if n >= 2 && line[n - 2] == 0xC2 && line[n - 1] == 0x85 {
                        line.removeLast(2)
                        return String(decoding: line, as: UTF8.self)
                    }
                    if n >= 3 && line[n - 3] == 0xE2 && line[n - 2] == 0x80 &&
                        (line[n - 1] == 0xA8 || line[n - 1] == 0xA9) {
                        line.removeLast(3)
                        return String(decoding: line, as: UTF8.self)
                    }
                }
            }
        }
    }

    public func makeAsyncIterator() -> AsyncIterator {
        return AsyncIterator(base: base.makeAsyncIterator())
    }
}

@available(macOS 12.0, *)
extension AsyncSequence where Element == UInt8 {
    public var lines: AsyncLineSequence<Self> {
        return AsyncLineSequence(base: self)
    }
}
