//---------------------------------------------------------------------------------
// LogDisabledTests.cpp
//---------------------------------------------------------------------------------
//
// A translation unit built like a release build of the engine: with
// ENGINE_LOGGING=0 (the default when NDEBUG is defined) every LOG_* macro
// must compile to nothing
//
#define ENGINE_LOGGING 0
#include "Log.h"

namespace
{
    int g_Evaluations = 0;

    int Expensive()
    {
        return ++g_Evaluations;
    }
} // namespace

int LoggingDisabledEvaluations()
{
    g_Evaluations = 0;
    LOG_TRACE("Release", "%d", Expensive());
    LOG_INFO("Release", "%d", Expensive());
    LOG_WARN("Release", "%d", Expensive());
    LOG_ERROR("Release", "%d", Expensive());
    return g_Evaluations;
}
