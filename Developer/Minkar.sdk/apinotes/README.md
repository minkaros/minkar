# API notes for Swift

How Swift sees the frameworks' Objective-C API (its names, nullability,
which methods are properties): the SDK installs each `<Framework>.apinotes`
next to that framework's headers, where clang reads it when Swift imports
the framework. Plain C and Objective-C builds do not read them.

Source: Swift 4.0.3 (`apinotes/` of swiftlang/swift, tag
`swift-4.0.3-RELEASE`, tarball SHA-256
026d596dd4a24580a5e442409e8c58259197bd73ddbb77e5aade96da982ea39b), the last
release that published them; Apache License 2.0 with the Runtime Library
Exception (LICENSE.swift.txt). Unchanged but for Minkar's additions, marked
as such (what headers cannot say, such as a class imported as not generic);
names they give to APIs Minkar does not have are ignored. Newer API (after
that release) is annotated in the headers themselves (NS_SWIFT_NAME and the
like).
