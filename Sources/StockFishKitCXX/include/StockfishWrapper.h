//
//  StockfishWrapper.h
//  StockFishKit_iOS SPM
//
//  Created by Soumit Kanti Saha on 2026-03-15.
//

#ifndef StockfishWrapper_h
#define StockfishWrapper_h

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize engine (once)
void stockfish_init(const char* _Nullable nnue_path);

// Start engine loop in background
void stockfish_start_loop(void);

// Send UCI command to engine
void stockfish_send_command(const char* _Nonnull command);

// Returns true if the engine has been initialised and not yet deleted.
bool stockfish_is_ready(void);

// Delete engine
void stockfish_delete(void);

// Set callback for engine output
typedef void (*stockfish_output_callback_t)(const char* _Nonnull);
void stockfish_set_output_callback(stockfish_output_callback_t _Nullable callback);

#ifdef __cplusplus
}
#endif

#endif // StockfishWrapper_h
