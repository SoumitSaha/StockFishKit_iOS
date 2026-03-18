//
//  StockFish_iOSDelegate.swift
//  StockFishKit_iOS SPM
//
//  Created by Soumit Kanti Saha on 2026-03-15.
//

import Foundation
import StockFishKitCXX

@MainActor
public final class StockFish_iOS: StockFish_iOSManaging {

    // MARK: - Singleton

    public static let shared = StockFish_iOS()

    // MARK: - Public Properties

    public weak var delegate: StockFish_iOSDelegate?
    public var onMessageReceived: ((String) -> Void)?

    // MARK: - Private State

    private var isEngineReady = false

    // MARK: - Init

    private init() {}

    // MARK: - Public API

    public func initEngine() {
        print(">>> initEngine called")
        stockfish_set_output_callback { cString in
            let message = String(cString: cString)
                .trimmingCharacters(in: .whitespacesAndNewlines)
            guard !message.isEmpty else { return }
            print(">>> RAW ENGINE OUTPUT: \(message)")

            Task { @MainActor in
                let manager = StockFish_iOS.shared
                if message == "readyok" {
                    manager.isEngineReady = true
                }
                manager.onMessageReceived?(message)
                manager.delegate?.stockFish_iOSManager(manager, didReceiveMessage: message)
            }
        }

        guard !stockfish_is_ready() else {
            print(">>> Engine already running")
            return
        }

        let nnuePath = Bundle.module.path(
            forResource: "nn-9a0cc2a62c52",
            ofType: "nnue"
        )
        print(">>> NNUE path: \(nnuePath ?? "NOT FOUND")")

        stockfish_init(nnuePath)
        stockfish_start_loop()

        // Poll until engine thread has set up streams and g_running is true
        waitForEngineReady()
    }

    private func waitForEngineReady() {
        if stockfish_is_ready() {
            print(">>> Engine ready, sending uci + isready")
            send("uci")
            send("isready")
        } else {
            // Check again in 100ms
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
                self.waitForEngineReady()
            }
        }
    }

    public func findBestMove(fen: String, depth: Int = 30) {
        guard stockfish_is_ready() else { return }
        guard isEngineReady else {
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                self.findBestMove(fen: fen, depth: depth)
            }
            return
        }
        send("isready")
        send("position fen \(fen)")
        send("go depth \(depth)")
    }

    public func findBestMove(moves: String, depth: Int = 30) {
        guard stockfish_is_ready(), isEngineReady else { return }
        send("isready")
        send("position startpos moves \(moves)")
        send("go depth \(depth)")
    }

    public func stopCurrentCommand() {
        guard stockfish_is_ready() else { return }
        send("stop")
    }

    public func stopEngine() {
        guard stockfish_is_ready() else { return }
        send("stop")
        isEngineReady = false
        stockfish_delete()
    }

    // MARK: - StockFish_iOSManaging

    public func send(_ command: String) {
        stockfish_send_command(command)
    }
}
