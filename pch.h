#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

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

#include "GWCA_BACKUP/gwca-src/Source/stdafx.h"
#include <cassert>
#include <cstdint>
#include <fixed_array.h>
#include <fixed_set.h>

#include "enum_as_int.h"