//---------------------------------------------------------------------------------
// Log.h
//---------------------------------------------------------------------------------
//
// Debug logging to standard output.
//
//   LOG_TRACE("Scripts", "%d instances", count);
//   LOG_INFO("Scene", "Loaded %s", path.c_str());
//   LOG_WARN("Assets", "Model %s not found", name.c_str());
//   LOG_ERROR("Save", "Could not write %s: %s", path.c_str(), error.c_str());
//
// Output:  [   12.345] INFO  Scene    | Loaded data/scenes/level_1.ubsave
//
// Messages use printf formatting (checked by GCC / Clang). The macros only
// exist in debug builds: in release builds (NDEBUG defined, e.g. CMake
// Release / Visual Studio Release) they compile to nothing and their
// arguments are not evaluated. Define ENGINE_LOGGING=1 to keep them in a
// release build, or ENGINE_LOGGING=0 to remove them from a debug build.
//
// On Windows the programs have no console (GUI subsystem): the first message
// opens one for standard output, and every message also goes to the
// debugger's output window (Visual Studio Output).
//
// Safe to call from any thread (the renderer's worker threads included).
//
#pragma once

#include <functional>
#include <string>

#ifndef ENGINE_LOGGING
#    ifdef NDEBUG
#        define ENGINE_LOGGING 0
#    else
#        define ENGINE_LOGGING 1
#    endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#    define ENGINE_PRINTF_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#    define ENGINE_PRINTF_FORMAT(fmt, args)
#endif

namespace Log
{
    enum class Level
    {
        Trace,
        Info,
        Warning,
        Error,
        Off
    };

    /**
     * \brief Messages below this level are dropped (default Trace: everything)
     */
    void SetLevel(Level minimum);
    Level GetLevel();

    /**
     * \brief Where finished lines go (default: standard output). Tests use
     *        it to capture or silence the log; nullptr restores stdout
     */
    using Sink = std::function<void(Level level, const std::string& line)>;
    void SetSink(Sink sink);

    /**
     * \brief Format and write one message (use the LOG_* macros instead, they
     *        disappear from release builds)
     */
    void Write(Level level, const char* category, const char* format, ...)
            ENGINE_PRINTF_FORMAT(3, 4);
} // namespace Log

#if ENGINE_LOGGING
#    define LOG_TRACE(category, ...) ::Log::Write(::Log::Level::Trace, category, __VA_ARGS__)
#    define LOG_INFO(category, ...) ::Log::Write(::Log::Level::Info, category, __VA_ARGS__)
#    define LOG_WARN(category, ...) ::Log::Write(::Log::Level::Warning, category, __VA_ARGS__)
#    define LOG_ERROR(category, ...) ::Log::Write(::Log::Level::Error, category, __VA_ARGS__)
#else
#    define LOG_TRACE(category, ...) ((void)0)
#    define LOG_INFO(category, ...) ((void)0)
#    define LOG_WARN(category, ...) ((void)0)
#    define LOG_ERROR(category, ...) ((void)0)
#endif
