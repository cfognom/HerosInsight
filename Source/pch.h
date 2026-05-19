#pragma once

#if defined(__clang__)
#define FORCE_INLINE [[gnu::always_inline]] [[gnu::gnu_inline]] extern inline

#elif defined(__GNUC__)
#define FORCE_INLINE [[gnu::always_inline]] inline

#elif defined(_MSC_VER)
#pragma warning(error : 4714)
#define FORCE_INLINE __forceinline

#else
#error Unsupported compiler
#endif

#if defined(_MSC_VER)
#define NO_INLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define NO_INLINE __attribute__((noinline))
#else
#define NO_INLINE
#endif

#include "GWCA_BACKUP/gwca-src/Source/stdafx.h"
#include <buffer.h>
#include <cassert>
#include <cstdint>
#include <fixed_set.h>
#include <make_color.h>

#include "enum_as_int.h"

#ifdef PRODUCTION_BUILD

#define ENABLE_SAFECALL

#else // NOT PRODUCTION_BUILD

#define EXPERIMENTAL_FEATURES
#define _PROFILING
// #define ENABLE_SAFECALL

#endif