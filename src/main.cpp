// main.cpp — Klaxon entry point.
//
// Boot order matters on the Wii U:
//   1. SD log first (so anything after is traceable on hardware)
//   2. App init/run/shutdown in a deterministic order
//   3. SDL_Quit() inside shutdown() performs the one and only ProcUI exit
//      handshake - returning from main() without it softlocks the console.
#include "app.hpp"
#include "common.hpp"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    bool logOk = LogInit();
    KLAXON_LOG("=== Klaxon boot (log %s) ===", logOk ? "open" : "UNAVAILABLE");

    int result = 0;
    {
        App app;
        if (!app.init()) {
            KLAXON_LOG("app.init() FAILED - running shutdown anyway so the OS gets its handshake");
            result = -1;
        } else {
            app.run();
        }
        app.shutdown();   // always runs; idempotent
    }

    KLAXON_LOG("clean exit (result=%d)", result);
    LogClose();
    return result;
}
