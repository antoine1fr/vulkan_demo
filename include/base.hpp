#pragma once

#include "config.hpp"

#if defined(DEBUG)
#define SUCCESS(x) assert((x) == VK_SUCCESS)
#else
#define SUCCESS(x) x
#endif

typedef size_t ResourceId;