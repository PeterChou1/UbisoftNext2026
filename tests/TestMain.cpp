//---------------------------------------------------------------------------------
// TestMain.cpp
//---------------------------------------------------------------------------------
#include "TestEnvironment.h"
#include "TestFramework.h"

#include <filesystem>

int main(int argc, char** argv)
{
    // Models (data/models) and scenes (data/scenes) are found relative to the
    // repository root, like the programs find them
    std::filesystem::current_path(REPO_ROOT);
    TestEnvironment::Init();
    return TestFramework::RunAll(argc, argv);
}
