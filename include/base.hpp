#pragma once

#include <cassert>

#include "config.hpp"

#if defined(DEBUG)
#define VK_CHECK(x) assert((x) == VK_SUCCESS)
#else
#define VK_CHECK(x) x
#endif

typedef size_t ResourceId;