#pragma once

#if defined(__APPLE__)
#define DEMO_BUILD_APPLE
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define DEMO_BUILD_WINDOWS
#elif defined(__linux__)
#define DEMO_BUILD_LINUX
#endif

#if defined(DEMO_BUILD_LINUX) || defined(DEMO_BUILD_APPLE)
#define DEMO_BUILD_UNIX
#endif