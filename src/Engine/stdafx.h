// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// Before any Windows header: keeps windows.h from defining min / max macros
#ifndef NOMINMAX
#    define NOMINMAX
#endif

#include "targetver.h"

#include <stdio.h>

#ifdef _WIN32
#    include <tchar.h>
#endif
