// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// The metrics API of XCTest on the reference platform (API 19): metrics,
// measure options and XCTestCase's measure(metrics:options:block:). The
// block runs iterationCount times; each run's time is printed, as
// swift-corelibs-xctest prints its own measurements. Only time is
// measured: the other metrics are accepted and named in the output.

public protocol XCTMetric: AnyObject {}

open class XCTClockMetric: XCTMetric {
    public init() {}
}

open class XCTCPUMetric: XCTMetric {
    public init() {}
    public init(limitingToCurrentProcess: Bool) {}
}

open class XCTMemoryMetric: XCTMetric {
    public init() {}
}

open class XCTStorageMetric: XCTMetric {
    public init() {}
}

open class XCTMeasureOptions {
    public struct InvocationOptions: OptionSet {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let manuallyStart = InvocationOptions(rawValue: 1 << 0)
        public static let manuallyStop = InvocationOptions(rawValue: 1 << 1)
    }

    open var invocationOptions: InvocationOptions = []
    open var iterationCount: Int = 5

    public init() {}

    open class var `default`: XCTMeasureOptions {
        return XCTMeasureOptions()
    }
}

extension XCTestCase {
    func minkarMeasure(metrics: [XCTMetric], options: XCTMeasureOptions, block: () -> Void) {
        var times: [Double] = []
        for _ in 0..<max(1, options.iterationCount) {
            let start = ProcessInfo.processInfo.systemUptime
            block()
            times.append(ProcessInfo.processInfo.systemUptime - start)
        }
        let average = times.reduce(0, +) / Double(times.count)
        let names = metrics.map { String(describing: type(of: $0)) }.joined(separator: ", ")
        print("Test Case '\(name)' measured [\(names)] average: \(String(format: "%.3f", average)), values: [\(times.map { String(format: "%.6f", $0) }.joined(separator: ", "))]")
    }
}
