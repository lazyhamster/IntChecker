// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <Shlwapi.h>

// Additional headers
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <memory>
#include <chrono>
#include <numeric>
#include <functional>
#include <random>

typedef std::vector<std::wstring> StringList;

#include <stdint.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define PATH_BUFFER_SIZE 4096
