#pragma once
#ifdef USE_DEBUG_LOG
#include <iostream>
#define LOG(x) std::cerr << __PRETTY_FUNCTION__ << ':' << __LINE__ << ": " << #x << " = " << ( x ) << std::endl
#else
#define LOG(x)
#endif
