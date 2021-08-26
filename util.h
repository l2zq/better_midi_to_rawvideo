#pragma once
#include <stdint.h>  // uintX_t
#include <stdlib.h>  //  size_t, NULL
#include <stdbool.h> // bool
#define NUL NULL     // beautiful
// clang-format off
typedef uint8_t  byte;
typedef uint16_t ui16;
typedef uint32_t ui32;
typedef uint64_t ui64;
typedef  int32_t si32;
typedef  int64_t si64;
typedef ui64 no_t; // noteid
typedef si64 tk_t; // tick_t
typedef ui64 tm_t; // time_t
// clang-format on
#if defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE)
#include <endian.h>
#define mem_be16(x) be16toh(*(ui16 *)(x))
#define mem_be32(x) be32toh(*(ui32 *)(x))
#else
// clang-format off
static inline ui16 mem_be16(byte *m) {
  ui16 x =     m[0];
  x = (x<<8) | m[1];
  return x;
}
static inline ui32 mem_be32(byte *m) {
  ui32 x =     m[0];
  x = (x<<8) | m[1];
  x = (x<<8) | m[2];
  x = (x<<8) | m[3];
  return x;
}
#endif