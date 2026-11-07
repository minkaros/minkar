// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// swift-hello: a Swift program in the image, for boottest's M3-swift gate.
// It checks a few things of the standard library and says how many held.

struct Check {
    let name: String
    let passed: Bool
}

final class Counter {
    private(set) var count = 0
    func add(_ n: Int) { count += n }
}

func run() -> [Check] {
    let counter = Counter()
    (1...10).forEach(counter.add)
    let words = "ñandú Swift Minkar".split(separator: " ").map(String.init)
    let sorted = [3, 1, 2].sorted()
    var seen = Set<String>()
    for w in words + words { seen.insert(w.lowercased()) }
    return [
        Check(name: "classes", passed: counter.count == 55),
        Check(name: "unicode", passed: words[0].count == 5 && words[0].uppercased() == "ÑANDÚ"),
        Check(name: "collections", passed: sorted == [1, 2, 3] && seen.count == 3),
    ]
}

let checks = run()
let failed = checks.filter { !$0.passed }.map(\.name)
if failed.isEmpty {
    print("swift-hello: Swift 6.0 on Minkar, \(checks.count) checks passed")
} else {
    print("swift-hello: failed: \(failed.joined(separator: ", "))")
}
