#pragma once

#ifdef HY_PLATFORM_WINDOWS
// Keep <Windows.h> from defining min/max macros (they break std::min/std::max/numeric_limits).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include <memory>
#include <functional>
#include <algorithm>

#include <string>
#include <fstream>
#include <sstream>

#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <Hydro/Core/Log.h>
