# StockFishKit_iOS

A Swift Package Manager (SPM) wrapper around the [Stockfish](https://stockfishchess.org) chess engine for iOS. This package exposes a clean Swift API over the Stockfish C++ engine, handles NNUE neural network loading, and manages the UCI communication protocol so you can integrate world-class chess analysis into any iOS app.

---

## Table of Contents

- [Requirements](#requirements)
- [Installation](#installation)
- [Project Structure](#project-structure)
- [How to Use](#how-to-use)
  - [Basic Setup](#basic-setup)
  - [Using the Delegate](#using-the-delegate)
  - [Finding Best Move](#finding-best-move)
  - [Stopping the Engine](#stopping-the-engine)
  - [Direct C++ Access](#direct-c-access)
- [How It Works Internally](#how-it-works-internally)
- [Known Issue & Fix — iOS Initialization Race Condition](#known-issue--fix--ios-initialization-race-condition)
  - [The Problem](#the-problem)
  - [Bug Localization](#bug-localization)
  - [Root Cause](#root-cause)
  - [The Race Condition](#the-race-condition)
  - [The Solution](#the-solution)
  - [Key Lessons](#key-lessons)

---

## Requirements

- iOS 14.0+
- Xcode 15+
- Swift 5.9+

---

## Installation

### Local Package (Development)

1. In Xcode, go to **File → Add Package Dependencies**
2. Click **Add Local** at the bottom left
3. Navigate to and select the `StockFishKit_iOS` folder
4. Click **Add Package**
5. Add `StockFishKit_iOS` to your app target

### Git (Production)

1. In Xcode, go to **File → Add Package Dependencies**
2. Enter your repository URL
3. Select the version tag
4. Add `StockFishKit_iOS` to your app target

---

## Project Structure

```
StockFishKit_iOS/
├── Package.swift
├── Sources/
│   ├── StockFishKitCXX/                  ← C++ target
│   │   ├── include/
│   │   │   ├── module.modulemap
│   │   │   ├── StockfishWrapper.h
│   │   │   └── StockFishKit_iOS.h
│   │   ├── Wrappers/
│   │   │   └── StockfishWrapper.cpp
│   │   └── StockFish_src/                ← Stockfish source files
│   │       ├── uci.cpp / uci.h
│   │       ├── bitboard.cpp / bitboard.h
│   │       ├── position.cpp / position.h
│   │       ├── engine.cpp / engine.h
│   │       ├── syzygy/
│   │       ├── incbin/
│   │       └── nnue/
│   └── StockFishKit_iOS/                 ← Swift target
│       ├── StockFish_iOS.swift
│       ├── StockFish_iOSDelegate.swift
│       └── Resources/
│           ├── nn-9a0cc2a62c52.nnue      ← Big network
│           └── nn-47fc8b7fff06.nnue      ← Small network
└── Tests/
    └── StockFishKit_iOSTests/
```

---

## How to Use

### Basic Setup

Import the package and initialize the engine when your view loads. The engine must be initialized before sending any commands.

```swift
import UIKit
import StockFishKit_iOS

class ViewController: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()

        // Set delegate before init
        StockFish_iOS.shared.delegate = self

        // Initialize the engine
        StockFish_iOS.shared.initEngine()
    }
}
```

`initEngine()` handles everything internally:
- Registers the output callback
- Resolves the bundled `.nnue` file path via `Bundle.module`
- Calls `stockfish_init()` with the correct path
- Starts the UCI engine loop on a background thread

---

### Using the Delegate

Conform to `StockFish_iOSDelegate` to receive engine output:

```swift
extension ViewController: StockFish_iOSDelegate {
    func stockFish_iOSManager(
        _ manager: any StockFish_iOSManaging,
        didReceiveMessage message: String
    ) {
        print("ENGINE: \(message)")

        if message.hasPrefix("bestmove") {
            // Parse the best move
            let parts = message.components(separatedBy: " ")
            if parts.count >= 2 {
                let bestMove = parts[1]
                print("Best move: \(bestMove)")
            }
        }

        if message == "readyok" {
            // Engine is ready to receive position and go commands
        }
    }
}
```

Alternatively, use the closure-based API:

```swift
StockFish_iOS.shared.onMessageReceived = { message in
    print("ENGINE: \(message)")
}
```

---

### Finding Best Move

Pass a FEN string and search depth. The engine will stream `info depth` lines and finish with a `bestmove` response via the delegate or closure.

```swift
@IBAction func analyzePosition(_ sender: Any) {
    // Standard starting position
    StockFish_iOS.shared.findBestMove(
        fen: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        depth: 20
    )
}
```

From a sequence of moves:

```swift
StockFish_iOS.shared.findBestMove(
    moves: "e2e4 e7e5 g1f3",
    depth: 20
)
```

**Important:** Do not include `"position fen"` in the FEN string — pass only the FEN itself. The wrapper adds the UCI prefix automatically.

---

### Stopping the Engine

Stop the current search:

```swift
StockFish_iOS.shared.stopCurrentCommand()
```

Fully shut down and release all resources:

```swift
StockFish_iOS.shared.stopEngine()
```

After `stopEngine()`, call `initEngine()` again to restart.

---

### Direct C++ Access

If you need direct access to UCI commands, import the C++ target instead:

```swift
import StockFishKitCXX

// Register callback
stockfish_set_output_callback { cString in
    let output = String(cString: cString)
    print(output)
}

// Initialize and start
stockfish_init(nil)
stockfish_start_loop()

// Send raw UCI commands
stockfish_send_command("isready")
stockfish_send_command("position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
stockfish_send_command("go depth 20")
```

---

### Engine Output Reference

| Output | Meaning |
|--------|---------|
| `uciok` | Engine acknowledged UCI mode |
| `readyok` | Engine ready to receive commands |
| `info depth N score cp X pv ...` | Search progress — depth, evaluation, principal variation |
| `bestmove e2e4` | Final answer — the best move found |
| `info string ERROR: ...` | NNUE or configuration error |

---

## How It Works Internally

The wrapper bridges Swift ↔ C++ using a three-layer architecture:

```
iOS App (Swift)
    ↓ import StockFishKit_iOS
StockFish_iOS.swift          ← Public Swift API, Bundle.module resource resolution
    ↓ import StockFishKitCXX
StockfishWrapper.cpp         ← C bridge, stream redirection, thread management
    ↓
Stockfish UCIEngine          ← Full Stockfish engine, UCI protocol, NNUE evaluation
```

**Stream redirection** is used to bridge Stockfish's `std::cout` output to the Swift callback. A custom `CallbackBuf` subclass intercepts every flush of `std::cout` and fires the registered C callback, which propagates up to Swift via `Task { @MainActor in ... }`.

**Command input** uses a thread-safe `QueueBuf` that feeds characters into `std::cin` on the engine thread. Swift calls `stockfish_send_command()` which enqueues the command and notifies the waiting engine thread.

---

## Known Issue & Fix — iOS Initialization Race Condition

### The Problem

The engine worked perfectly on an **M4 Mac Mini** during development and in XCTest, but crashed on a real **iPhone 14 Pro** with the assertion:

```
assert(count<Pt>(c) == 1)
```

This assert checks that exactly one king exists on each side of the board — a fundamental position validity check inside Stockfish's `pos_is_ok()`. The crash occurred during engine **initialization itself**, before any chess commands were sent.

---

### Bug Localization

`printf` statements were added at every stage of the initialization pipeline:

```
Stage 1: stockfish_init() .................. ✅ complete
Stage 2: engine thread started ............. ✅
Stage 3: UCIEngine constructor entered ..... ✅
Stage 4: get_default_networks() ............ ✅ complete
Stage 5: Engine constructor entered ........ ✅
Stage 6: pos.set(StartFEN) ................. ❌ CRASH
```

The crash was inside `Position::set()`, called at the end of the `Engine` constructor to set up the initial board state.

Thread ID logging on `Bitboards::init()` and `Position::init()` revealed the key insight:

```
>>> Bitboards::init() called from thread: 6134181888  ← engine thread
>>> Engine boot delay complete                         ← main thread (1s delay finished)
>>> Position::init() called from thread: 6134181888   ← engine thread
```

Both init functions were being called **lazily inside the `Engine` constructor via static initializers** — not before the constructor ran.

---

### Root Cause

Reading the Stockfish source code call chain:

```
UCIEngine(argc, argv)
  └── Engine(argv[0])
        └── networks(numaContext, get_default_networks())  ← loads NNUE
        └── pos.set(StartFEN)                              ← sets up board
              └── set_state()
                    └── attackers_to()
                          └── attacks_bb()   ← requires Bitboards tables
```

`pos.set()` internally calls `attacks_bb()` — a function that relies on pre-computed bitboard attack tables populated by `Bitboards::init()`. These tables were not guaranteed to be ready when `pos.set()` executed on the engine thread.

---

### The Race Condition

**On M4 Mac Mini:**
Static initializers complete near-instantly. By the time `pos.set()` executes, all attack tables are fully populated. No crash.

**On iPhone 14 Pro:**
The slower processor and memory system means static initializers take measurably longer. `pos.set()` executes while the bitboard tables are still being written. `attacks_bb()` reads partially-initialized memory. The king count check fails → assertion crash.

This is a classic **static initialization order fiasco** — the order in which static initializers complete across compilation units is not guaranteed, and on slower hardware the race condition becomes visible.

---

### The Solution

The fix required two changes working together:

**Part 1 — Force initialization on the main thread before the engine thread starts:**

```cpp
void stockfish_init(const char* nnue_path) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (g_initialized) return;

    if (nnue_path) {
        g_nnue_path = nnue_path;
    }

    // Force on calling thread (main thread) BEFORE engine thread starts.
    // This ensures attack tables are fully populated before pos.set() runs.
    Stockfish::Bitboards::init();
    Stockfish::Position::init();

    g_initialized = true;
}
```

**Part 2 — Make init functions idempotent (safe to call multiple times):**

Since `Engine` constructor also triggers these inits internally, calling them twice would corrupt the tables. A `static bool` guard was added to each:

```cpp
// bitboard.cpp
void Bitboards::init() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    // ... existing initialization code unchanged
}

// position.cpp
void Position::init() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    // ... existing initialization code unchanged
}
```

**Result — deterministic execution order:**

```
Main Thread                     Engine Thread
────────────────────            ──────────────────────
stockfish_init()
  Bitboards::init() ✅ runs
  Position::init()  ✅ runs
  g_initialized = true

stockfish_start_loop()
  spawn engine thread ──────→  Bitboards::init() → skipped ✅
                                Position::init()  → skipped ✅
                                pos.set(StartFEN) → tables ready ✅
                                engine.loop()     → running ✅
```

---

### Key Lessons

**Never assume initialization order across threads.** What works on fast hardware may fail on slower devices due to race conditions in static initializers. Always explicitly initialize shared state on a known thread before spawning worker threads that depend on it.

**Systematic logging beats guessing.** The crash was narrowed from "somewhere in engine initialization" to the exact line `pos.set()` by adding `printf` at every stage of the call chain. Thread ID logging then revealed which thread was running which code and in what order.

**Make init functions idempotent.** Any initialization function that might be called from multiple code paths — including third-party libraries — should be safe to call multiple times. A simple `static bool` guard achieves this with zero overhead after the first call.

**Cross-platform timing differences are real.** An M4 Mac Mini is roughly 3–4× faster than an iPhone 14 Pro in single-thread performance. Code that races on iPhone can appear completely stable on Mac, making the bug invisible during development.

**Read the source.** The breakthrough came from reading the Stockfish `engine.cpp` source and tracing the exact call chain from `UCIEngine` constructor → `Engine` constructor → `pos.set()` → `set_state()` → `attackers_to()` → `attacks_bb()`. Without understanding what each layer called internally, the fix would have remained guesswork.

---

## License

Stockfish is licensed under the [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.en.html). This wrapper package is provided under the same license terms.
