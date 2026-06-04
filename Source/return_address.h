#pragma once

// clang-format off
#if defined(_MSC_VER)
    #include<intrin.h>
    #define RETURN_ADDRESS() _ReturnAddress()

#elif defined(__GNUC__) || defined(__clang__) // GCC / Clang
    #define RETURN_ADDRESS() __builtin_extract_return_addr(__builtin_return_address(0))

#else // Fallback (not portable everywhere)
    #define RETURN_ADDRESS() ((void *)0)

#endif
// clang-format on