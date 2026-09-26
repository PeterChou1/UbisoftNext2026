//---------------------------------------------------------------------------------
// LogTests.cpp
//---------------------------------------------------------------------------------
//
// The debug logging utility (Engine/Log.h): formatting, levels, sinks,
// threads (through Log::Write, present in every build), and the engine's
// own LOG_* messages (debug builds only)
//
#include "GameManager.h"
#include "Log.h"
#include "TestEnvironment.h"
#include "TestFramework.h"

#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern GameManager GameSceneManager;

namespace
{
    struct Captured
    {
        Log::Level Level;
        std::string Line;
    };

    // Captures the log for one test, restores the silent test sink after
    struct Capture
    {
        std::vector<Captured> Lines;
        std::mutex Mutex;
        Capture()
        {
            Log::SetLevel(Log::Level::Trace);
            Log::SetSink([this](Log::Level level, const std::string& line) {
                std::lock_guard<std::mutex> lock(Mutex);
                Lines.push_back({level, line});
            });
        }
        ~Capture()
        {
            Log::SetLevel(Log::Level::Trace);
            Log::SetSink([](Log::Level, const std::string&) {});
        }
        bool Has(const std::string& fragment)
        {
            std::lock_guard<std::mutex> lock(Mutex);
            for (const auto& c : Lines)
            {
                if (c.Line.find(fragment) != std::string::npos)
                    return true;
            }
            return false;
        }
    };
} // namespace

TEST_CASE("Log: messages are formatted with time, level, category and printf arguments")
{
    Capture log;
    Log::Write(Log::Level::Info, "Scene", "Loaded %s with %d objects (%.1f s)", "level_1", 12, 0.25);
    REQUIRE(log.Lines.size() == 1);
    const std::string& line = log.Lines[0].Line;
    CHECK(log.Lines[0].Level == Log::Level::Info);
    CHECK(line.front() == '[');
    CHECK(line.find("] INFO  Scene    | Loaded level_1 with 12 objects (0.2 s)") != std::string::npos ||
          line.find("] INFO  Scene    | Loaded level_1 with 12 objects (0.3 s)") != std::string::npos);

    Log::Write(Log::Level::Warning, "Assets", "no arguments");
    Log::Write(Log::Level::Error, "Save", "%s", "failed");
    Log::Write(Log::Level::Trace, "Scripts", "%zu", size_t(3));
    CHECK(log.Has("WARN  Assets   | no arguments"));
    CHECK(log.Has("ERROR Save     | failed"));
    CHECK(log.Has("TRACE Scripts  | 3"));

    // Long messages are not truncated
    std::string longText(5000, 'x');
    Log::Write(Log::Level::Info, "Long", "%s!", longText.c_str());
    CHECK(log.Lines.back().Line.size() > 5000);
    CHECK(log.Lines.back().Line.back() == '!');
}

TEST_CASE("Log: messages below the level are dropped")
{
    Capture log;
    Log::SetLevel(Log::Level::Warning);
    CHECK(Log::GetLevel() == Log::Level::Warning);
    Log::Write(Log::Level::Trace, "A", "trace");
    Log::Write(Log::Level::Info, "A", "info");
    Log::Write(Log::Level::Warning, "A", "warn");
    Log::Write(Log::Level::Error, "A", "error");
    REQUIRE(log.Lines.size() == 2);
    CHECK(log.Lines[0].Level == Log::Level::Warning);
    CHECK(log.Lines[1].Level == Log::Level::Error);

    Log::SetLevel(Log::Level::Off);
    Log::Write(Log::Level::Error, "A", "silenced");
    CHECK_EQ(log.Lines.size(), size_t(2));
}

TEST_CASE("Log: safe from many threads at once")
{
    Capture log;
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t)
        threads.emplace_back([t] {
            for (int i = 0; i < 200; ++i)
                Log::Write(Log::Level::Info, "Thread", "thread %d message %d", t, i);
        });
    for (auto& thread : threads)
        thread.join();
    CHECK_EQ(log.Lines.size(), size_t(1600));
    // Every line is whole (no interleaving inside a line)
    for (const auto& c : log.Lines)
        CHECK(c.Line.find("| thread ") != std::string::npos && c.Line.find("message") != std::string::npos);
}

TEST_CASE("Log: the engine reports loads and scene changes (debug builds only)")
{
    Capture log;
    std::string error;
    CHECK(!GameSceneManager.LoadGame("data/scenes/does_not_exist.ubsave", error));
    CHECK(GameSceneManager.LoadGame(GameManager::ScenePath("level_1"), error));
#if ENGINE_LOGGING
    CHECK(log.Has("ERROR Load"));
    CHECK(log.Has("does_not_exist.ubsave"));
    CHECK(log.Has("INFO  Scene    | Active scene: Play"));
    CHECK(log.Has("INFO  Load     | Loaded data/scenes/level_1.ubsave"));
#else
    // Release build: the engine's LOG_* calls are compiled out
    CHECK(log.Lines.empty());
#endif
}

// Defined in LogDisabledTests.cpp, compiled with ENGINE_LOGGING=0
int LoggingDisabledEvaluations();

TEST_CASE("Log: with logging compiled out the macros do nothing, arguments are not evaluated")
{
    Capture log;
    CHECK_EQ(LoggingDisabledEvaluations(), 0);
    CHECK(log.Lines.empty());
}
