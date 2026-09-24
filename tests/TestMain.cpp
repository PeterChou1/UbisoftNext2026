//---------------------------------------------------------------------------------
// TestMain.cpp
//---------------------------------------------------------------------------------
#include "ECSManager.h"
#include "TestFramework.h"

// The engine sources refer to the global ECS instance (see GameTest.cpp)
ECSManager ECS;

int main(int argc, char** argv)
{
    ECS.Init();
    return TestFramework::RunAll(argc, argv);
}
