//---------------------------------------------------------------------------------
// TestCompat.h
//---------------------------------------------------------------------------------
//
// Force-included into every translation unit of the test executable.
//
// The game is developed with MSVC (Windows) and AppleClang (MacOS). Some game
// sources rely on standard headers being included transitively by those
// toolchains. This header provides them so the unchanged game sources also
// compile with GCC/libstdc++ (e.g. on a Linux CI machine). It is never part of
// the game build itself.
//
#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__GLIBCXX__)
// libstdc++ does not declare the C99 float overloads inside namespace std
namespace std
{
    using ::acosf;
    using ::cosf;
    using ::sinf;
    using ::sqrtf;
    using ::tanf;
} // namespace std
#endif
