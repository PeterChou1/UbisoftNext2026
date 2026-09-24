//---------------------------------------------------------------------------------
// app.h (test stub)
//---------------------------------------------------------------------------------
//
// Some engine sources used by the tests include the ContestAPI "app.h" header
// without calling into it. The real header pulls in OpenGL/GLUT which is not
// needed (nor available) for headless unit tests, so the tests use this stub.
//
#pragma once
#include "AppSettings.h"
