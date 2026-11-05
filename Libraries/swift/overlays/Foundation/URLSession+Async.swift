// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// API 21 (12.0): URLSession's data loads with async/await, on its data
// tasks; cancelling the calling task cancels the load.

@_exported import Foundation

@available(macOS 12.0, *)
extension URLSession {
    public func data(for request: URLRequest,
                     delegate: URLSessionTaskDelegate? = nil) async throws -> (Data, URLResponse) {
        final class Box: @unchecked Sendable { var task: URLSessionDataTask? }
        let box = Box()
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { done in
                let made: URLSessionDataTask? = self.dataTask(with: request) { data, response, error in
                    if let error = error {
                        done.resume(throwing: error)
                    } else if let response = response {
                        done.resume(returning: (data ?? Data(), response))
                    } else {
                        done.resume(throwing: URLError(.badServerResponse))
                    }
                }
                guard let task = made else {
                    done.resume(throwing: URLError(.unknown))
                    return
                }
                box.task = task
                task.resume()
            }
        } onCancel: {
            box.task?.cancel()
        }
    }

    public func data(from url: URL,
                     delegate: URLSessionTaskDelegate? = nil) async throws -> (Data, URLResponse) {
        return try await data(for: URLRequest(url: url), delegate: delegate)
    }
}
