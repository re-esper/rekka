#pragma once

#define REKKA_VERSION		"0.1.0"

#define NS_REK_BEGIN		namespace rekka {
#define NS_REK_END			}
#define USING_NS_REK		using namespace rekka

#define SAFE_DELETE(p)           do { if (p) { delete (p); (p) = nullptr; } } while(0)
#define SAFE_DELETE_ARRAY(p)     do { if (p) { delete[] (p); (p) = nullptr; } } while(0)
#define SAFE_FREE(p)             do { if (p) { free(p); (p) = nullptr; } } while(0)

#include "SDL.h"
#include "xgpu.h"

#ifdef WIN32
#include "glew.h"
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Initialization.h"
#include "js/Conversions.h"

#include <string>
#include <vector>
#include <algorithm>

#include <assert.h>

#include "utils.h"