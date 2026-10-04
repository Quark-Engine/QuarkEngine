#ifndef __VERSION_H__
#define __VERSION_H__

static inline constexpr const char* QUARK_ENGINE_VERSION = "v1.0.2";

#ifndef QUARK_ENGINE_BUILD
#define QUARK_ENGINE_BUILD "unknown"
#endif

#ifndef QUARK_ENGINE_BUILD_DATE
#define QUARK_ENGINE_BUILD_DATE "unknown"
#endif

static inline constexpr const char* QUARK_ENGINE_BUILD_NUMBER = QUARK_ENGINE_BUILD;
static inline constexpr const char* QUARK_ENGINE_BUILD_DATE_STRING = QUARK_ENGINE_BUILD_DATE;

#endif // __VERSION_H__
