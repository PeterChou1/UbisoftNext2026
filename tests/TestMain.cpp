//---------------------------------------------------------------------------------
// TestMain.cpp
//---------------------------------------------------------------------------------
#include "Log.h"
#include "TestEnvironment.h"
#include "TestFramework.h"

#include <cstdlib>
#include <filesystem>

int main(int argc, char** argv)
{
    // Models (data/models) and scenes (data/scenes) are found relative to the
    // repository root, like the programs find them
    std::filesystem::current_path(REPO_ROOT);
    // The engine logs to stdout in debug builds: keep the test output
    // readable unless UBI_TEST_LOG=1 asks for it
    if (std::getenv("UBI_TEST_LOG") == nullptr)
        Log::SetSink([](Log::Level, const std::string&) {});
    TestEnvironment::Init();
    return TestFramework::RunAll(argc, argv);
}
