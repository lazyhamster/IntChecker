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

typedef std::vector<std::wstring> StringList;

using namespace std;

#include <stdint.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define SETTINGS_KEY_REGISTRY L"IntChecker"
#define PATH_BUFFER_SIZE 4096
