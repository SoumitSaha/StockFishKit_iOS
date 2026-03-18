//
//  StockfishWrapper.cpp
//  StockFishKit_iOS
//
//  Created by Soumit Kanti Saha on 2026-03-15.
//

#include "StockfishWrapper.h"
#include "uci.h"
#include "bitboard.h"
#include "position.h"

#include <thread>
#include <mutex>
#include <string>
#include <iostream>
#include <sstream>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <future>

using namespace Stockfish;

// MARK: - QueueBuf
// A streambuf that acts as a thread-safe queue of characters,
// used to feed commands into std::cin on the engine thread.

struct QueueBuf : public std::streambuf {
    std::deque<char>        char_queue;
    std::mutex              m;
    std::condition_variable cv;
    std::atomic<bool>       closing { false };

    void add_cmd(const std::string& s) {
        {
            std::lock_guard<std::mutex> l(m);
            for (char c : s)  char_queue.push_back(c);
            char_queue.push_back('\n');
        }
        cv.notify_one();
    }

    void close() {
        {
            std::lock_guard<std::mutex> l(m);
            closing = true;
        }
        cv.notify_all();
    }

    int underflow() override {
        std::unique_lock<std::mutex> l(m);
        cv.wait(l, [this] { return !char_queue.empty() || closing.load(); });
        if (char_queue.empty()) return traits_type::eof();
        return (unsigned char)char_queue.front();
    }

    int uflow() override {
        std::unique_lock<std::mutex> l(m);
        cv.wait(l, [this] { return !char_queue.empty() || closing.load(); });
        if (char_queue.empty()) return traits_type::eof();
        int res = (unsigned char)char_queue.front();
        char_queue.pop_front();
        return res;
    }
};

// MARK: - CallbackBuf
// A streambuf that intercepts std::cout and fires the Swift callback
// on every flushed line.

class CallbackBuf : public std::stringbuf {
public:
    explicit CallbackBuf(stockfish_output_callback_t cb) : output_cb(cb) {}

protected:
    int sync() override {
        std::string s = str();
        if (!s.empty() && output_cb) {
            output_cb(s.c_str());
        }
        str("");
        return 0;
    }

private:
    stockfish_output_callback_t output_cb;
};

// MARK: - Global State

static QueueBuf*                    g_cin_buf         = nullptr;
static std::streambuf*              g_old_cout        = nullptr;
static std::streambuf*              g_old_cin         = nullptr;
static CallbackBuf*                 g_out_buf         = nullptr;
static stockfish_output_callback_t  g_output_callback = nullptr;
static std::atomic<bool>            g_running         { false };
static std::atomic<bool>            g_initialized     { false };
static std::mutex                   g_state_mutex;
static std::string                  g_nnue_path;       // ← moved here, file scope

// MARK: - C API

extern "C" {

void stockfish_init(const char* nnue_path) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (g_initialized) return;

    if (nnue_path) {
        g_nnue_path = nnue_path;
    }

    // Force static initializers to run on main thread
    // before engine thread starts — prevents lazy init
    // race condition on iPhone's slower processor
    Stockfish::Bitboards::init();
    Stockfish::Position::init();

    printf(">>> stockfish_init: complete\n");
    fflush(stdout);

    g_initialized = true;
}

void stockfish_start_loop(void) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (g_running) return;
    g_running = true;

    stockfish_output_callback_t cb = g_output_callback;

    std::thread([cb]() {

//        printf(">>> engine thread: Bitboards::init()\n");
//        fflush(stdout);
//        Stockfish::Bitboards::init();
//
//        printf(">>> engine thread: Position::init()\n");
//        fflush(stdout);
//        Stockfish::Position::init();

        printf(">>> engine thread: creating UCIEngine\n");
        fflush(stdout);

        std::string binaryPath = "stockfish";
        std::string evalFileArg = "";
        std::string evalFileSmallArg = "";

        if (!g_nnue_path.empty()) {
            std::string dir = g_nnue_path.substr(0, g_nnue_path.find_last_of('/'));
            binaryPath = dir + "/";
            evalFileArg = "setoption name EvalFile value " + dir + "/nn-9a0cc2a62c52.nnue";
            evalFileSmallArg = "setoption name EvalFileSmall value " + dir + "/nn-47fc8b7fff06.nnue";
        }

        printf(">>> engine thread: binaryPath = %s\n", binaryPath.c_str());
        fflush(stdout);

        std::vector<char> pathBuf(binaryPath.begin(), binaryPath.end());
        pathBuf.push_back('\0');
        char* argv[] = { pathBuf.data(), nullptr };

        Stockfish::UCIEngine engine(1, argv);

        printf(">>> engine thread: UCIEngine created\n");
        fflush(stdout);

        g_out_buf  = new CallbackBuf(cb);
        g_cin_buf  = new QueueBuf();
        g_old_cout = std::cout.rdbuf(g_out_buf);
        g_old_cin  = std::cin.rdbuf(g_cin_buf);

        printf(">>> engine thread: streams redirected\n");
        fflush(stdout);

        // Inject full NNUE paths now that streams are redirected
        if (!evalFileArg.empty()) {
            g_cin_buf->add_cmd(evalFileArg);
            g_cin_buf->add_cmd(evalFileSmallArg);
        }

        printf(">>> engine thread: starting loop\n");
        fflush(stdout);

        engine.loop();

        std::cout.rdbuf(g_old_cout);
        std::cin.rdbuf(g_old_cin);

        delete g_out_buf;  g_out_buf  = nullptr;
        delete g_cin_buf;  g_cin_buf  = nullptr;
        g_old_cout = nullptr;
        g_old_cin  = nullptr;
        g_running  = false;

    }).detach();
}

void stockfish_send_command(const char* command) {
    if (!command) return;
    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (g_cin_buf) {
        g_cin_buf->add_cmd(command);
    }
}

bool stockfish_is_ready(void) {
    return g_initialized && g_running;
}

void stockfish_delete(void) {
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        if (g_cin_buf) {
            g_cin_buf->add_cmd("quit");
            g_cin_buf->close();
        }
    }

    // Reset init flag so the engine can be re-created if needed.
    g_initialized = false;
    g_output_callback = nullptr;
}

void stockfish_set_output_callback(stockfish_output_callback_t callback) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    g_output_callback = callback;

    // If the output buffer is already live (loop started before callback
    // was set), hot-swap it.
    if (g_out_buf) {
        g_out_buf->~CallbackBuf();
        new (g_out_buf) CallbackBuf(callback);
    }
}

} // extern "C"
