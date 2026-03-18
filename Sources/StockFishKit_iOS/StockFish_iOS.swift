//
//  StockFish_iOSDelegate.swift
//  StockFishKit_iOS SPM
//
//  Created by Soumit Kanti Saha on 2026-03-15.
//

import Foundation

/// Describes the engine manager passed back in delegate callbacks.
@MainActor
public protocol StockFish_iOSManaging: AnyObject {
    func send(_ command: String)
    var onMessageReceived: ((String) -> Void)? { get set }
}

/// Delegate protocol to receive updates from the Stockfish engine.
@MainActor
public protocol StockFish_iOSDelegate: AnyObject {
    func stockFish_iOSManager(_ manager: any StockFish_iOSManaging, didReceiveMessage message: String)
}
