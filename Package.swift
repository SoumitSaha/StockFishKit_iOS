// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "StockFishKit_iOS",
    platforms: [
        .iOS(.v14)
    ],
    products: [
        .library(
            name: "StockFishKit_iOS",
            targets: ["StockFishKit_iOS"]
        )
    ],
    targets: [

        // ── C++ target (Stockfish engine + wrapper) ───────────────────
        .target(
            name: "StockFishKitCXX",
            path: "Sources/StockFishKitCXX",
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("StockFish_src"),
                .headerSearchPath("StockFish_src/nnue"),
                .headerSearchPath("StockFish_src/nnue/features"),
                .headerSearchPath("StockFish_src/syzygy"),
                .headerSearchPath("StockFish_src/incbin"),
                .headerSearchPath("include"),
                .define("IS_64BIT"),
                .define("USE_POPCNT"),
                .define("NNUE_EMBEDDING_OFF"),
                .unsafeFlags([
                    "-std=c++17",
                    "-Wno-comma",
                    "-Wno-unused-parameter"
                ])
            ]
        ),

        // ── Swift target (public API) ─────────────────────────────────
        .target(
            name: "StockFishKit_iOS",
            dependencies: ["StockFishKitCXX"],
            path: "Sources/StockFishKit_iOS",
            resources: [
                .copy("Resources/nn-9a0cc2a62c52.nnue"),
                .copy("Resources/nn-47fc8b7fff06.nnue")
            ],
            swiftSettings: [
                .interoperabilityMode(.Cxx)
            ]
        )
    ],
    cxxLanguageStandard: .cxx17
)