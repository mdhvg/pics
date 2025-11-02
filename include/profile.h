#pragma once

#if ENABLE_PROFILING == 1
#include "tracy/Tracy.hpp"
#else
#define ZoneScoped
#define ZoneScopedN(x)
#define FrameMark
#define TracyPlot(x, y)
#endif
