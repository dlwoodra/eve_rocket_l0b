#ifndef BYTESWAP_HPP
#define BYTESWAP_HPP

#include <cstdint>

// Check if the compiler supports __builtin_bswap32
#if defined(__has_builtin)
    #if __has_builtin(__builtin_bswap32)
        #define HAS_BUILTIN_BSWAP32 1
    #endif
    #if __has_builtin(__builtin_bswap16)
        #define HAS_BUILTIN_BSWAP16 1
    #endif
#elif defined(__GNUC__)
    #if (__GNUC__ >= 4) // __builtin_bswap32 is supported since GCC 4.x
        #define HAS_BUILTIN_BSWAP32 1
        #define HAS_BUILTIN_BSWAP16 1
    #endif
#endif


// Define a byte-swapping function
inline uint32_t byteswap_32(uint32_t value) {
#if defined(HAS_BUILTIN_BSWAP32)
    return __builtin_bswap32(value);
#else
    // Fallback: manual byte swap using bit shifts
    return ((value >> 24) & 0x000000FF) |
           ((value >> 8)  & 0x0000FF00) |
           ((value << 8)  & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
#endif
}

// Define a byte-swapping function for 16-bit
inline uint16_t byteswap_16(uint16_t value) {
#if defined(HAS_BUILTIN_BSWAP16)
    return __builtin_bswap16(value);
#else
    // Fallback: manual byte swap using bit shifts
    return ((value >> 8) & 0x00FF) |
           ((value << 8) & 0xFF00);
#endif
}

#endif //BYTESWAP_HPP