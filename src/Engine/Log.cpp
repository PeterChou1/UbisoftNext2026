#include "Log.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <vector>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#endif

namespace Log
{
    namespace
    {
        struct State
        {
            std::mutex Mutex;
            Level Minimum = Level::Trace;
            Sink Output;
            bool ConsoleReady = false;
            std::chrono::steady_clock::time_point Start = std::chrono::steady_clock::now();
        };

        State& GetState()
        {
            static State state;
            return state;
        }

        // GUI programs on Windows start without a console: open one so
        // standard output is visible (only when logging is actually used)
        void EnsureConsole(State& state)
        {
            if (state.ConsoleReady)
                return;
            state.ConsoleReady = true;
#if defined(_WIN32)
            if (GetConsoleWindow() == nullptr && AllocConsole())
            {
                FILE* stream = nullptr;
                freopen_s(&stream, "CONOUT$", "w", stdout);
                freopen_s(&stream, "CONOUT$", "w", stderr);
            }
#endif
        }

        void WriteToStdout(State& state, Level level, const std::string& line)
        {
            EnsureConsole(state);
            std::fputs(line.c_str(), stdout);
            std::fputc('\n', stdout);
            // Warnings and errors must not sit in a buffer if the program dies
            if (level >= Level::Warning)
                std::fflush(stdout);
#if defined(_WIN32)
            OutputDebugStringA((line + "\n").c_str());
#endif
        }

        const char* LevelName(Level level)
        {
            switch (level)
            {
            case Level::Trace:
                return "TRACE";
            case Level::Info:
                return "INFO ";
            case Level::Warning:
                return "WARN ";
            case Level::Error:
                return "ERROR";
            case Level::Off:
            default:
                return "OFF  ";
            }
        }
    } // namespace

    void SetLevel(Level minimum)
    {
        State& state = GetState();
        std::lock_guard<std::mutex> lock(state.Mutex);
        state.Minimum = minimum;
    }

    Level GetLevel()
    {
        State& state = GetState();
        std::lock_guard<std::mutex> lock(state.Mutex);
        return state.Minimum;
    }

    void SetSink(Sink sink)
    {
        State& state = GetState();
        std::lock_guard<std::mutex> lock(state.Mutex);
        state.Output = std::move(sink);
    }

    void Write(Level level, const char* category, const char* format, ...)
    {
        State& state = GetState();
        {
            std::lock_guard<std::mutex> lock(state.Mutex);
            if (level < state.Minimum || level == Level::Off)
                return;
        }

        // Message
        va_list args;
        va_start(args, format);
        va_list copy;
        va_copy(copy, args);
        int length = std::vsnprintf(nullptr, 0, format, copy);
        va_end(copy);
        std::vector<char> message(static_cast<size_t>(length > 0 ? length : 0) + 1, '\0');
        if (length > 0)
            std::vsnprintf(message.data(), message.size(), format, args);
        va_end(args);

        // [ seconds] LEVEL Category | message
        double seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - state.Start)
                        .count();
        char prefix[64];
        std::snprintf(prefix,
                      sizeof(prefix),
                      "[%9.3f] %s %-8s | ",
                      seconds,
                      LevelName(level),
                      category != nullptr ? category : "");
        std::string line = std::string(prefix) + message.data();

        std::lock_guard<std::mutex> lock(state.Mutex);
        if (state.Output)
            state.Output(level, line);
        else
            WriteToStdout(state, level, line);
    }
} // namespace Log
