#pragma once

#if   defined(WIN64)
#include "siinstrreloc64.h"
#elif defined(WIN32)
#include "siinstrreloc32.h"
#else
#error Unsupported platform!
#endif
