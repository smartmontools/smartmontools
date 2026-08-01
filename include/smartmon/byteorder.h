/*
 * byteorder.h - Types and functions for LE and BE integers
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2025-26 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SMARTMON_BYTEORDER_H
#define SMARTMON_BYTEORDER_H

#include <smartmon/smartmon_defs.h>

#include <stdint.h>
#include <stdlib.h>

#include <utility>

namespace smartmon {

// Little/Big Endian compile time constant
#ifdef SMARTMON_WORDS_BIGENDIAN
constexpr bool byteorder_is_big_endian = true;
#else
constexpr bool byteorder_is_big_endian = false;
#endif

// Return byte swapped values of int*_t

constexpr uint16_t byteswap_uint16(uint16_t x)
{
  return x << 8 | x >> 8;
}

constexpr uint32_t byteswap_uint32(uint32_t x)
{
  return   (x & 0x000000ff) << 24 | (x & 0x0000ff00) <<  8
         | (x & 0x00ff0000) >>  8 | (x & 0xff000000) >> 24;
}

constexpr uint64_t byteswap_uint64(uint64_t x)
{
  return   (x & 0x00000000000000ffULL) << 56 | (x & 0x000000000000ff00ULL) << 40
         | (x & 0x0000000000ff0000ULL) << 24 | (x & 0x00000000ff000000ULL) <<  8
         | (x & 0x000000ff00000000ULL) >>  8 | (x & 0x0000ff0000000000ULL) >> 24
         | (x & 0x00ff000000000000ULL) >> 40 | (x & 0xff00000000000000ULL) >> 56;
}

// Swap bytes of aligned uint*_t in-place

static inline void byteswap_inplace(uint16_t & x)
{
  x = byteswap_uint16(x);
}

static inline void byteswap_inplace(uint32_t & x)
{
  x = byteswap_uint32(x);
}

static inline void byteswap_inplace(uint64_t & x)
{
  x = byteswap_uint64(x);
}

// Swap bytes of all elements of aligned arrays of uint*_t

template <typename T>
static inline void byteswap_array_inplace(T * p, size_t size)
{
  for (size_t i = 0; i < size; i++)
    byteswap_inplace(p[i]);
}

template <typename T, size_t SIZE>
static inline void byteswap_array_inplace(T (& a)[SIZE])
{
  byteswap_array_inplace(a, SIZE);
}

// Swap adjacent bytes of arrays of uint8_t

static inline void byteswap_array_16_inplace(uint8_t * p, size_t size)
{
  for (size_t i = 0; i + 1 < size; i += 2)
    std::swap(p[i], p[i + 1]);
}

template <size_t SIZE>
static inline void byteswap_array_16_inplace(uint8_t (& a)[SIZE])
{
  SMARTMON_STATIC_ASSERT(SIZE % 2 == 0);
  byteswap_array_16_inplace(a, SIZE);
}

// Unaligned Little Endian unsigned integers
struct uile16_t { uint8_t b[2]; };
struct uile32_t { uint8_t b[4]; };
struct uile64_t { uint8_t b[8]; };
struct uile128_t { uile64_t lo; uile64_t hi; };

// Unaligned Big Endian unsigned integers
struct uibe16_t { uint8_t b[2]; };
struct uibe32_t { uint8_t b[4]; };
struct uibe64_t { uint8_t b[8]; };

// uile*_t -> uint*_t

constexpr uint16_t uile16_to_uint(uile16_t x)
{
  return (x.b[1] << 8) | x.b[0];
}

constexpr uint32_t uile32_to_uint(uile32_t x)
{
  return   ((uint32_t)x.b[3] << 24) | ((uint32_t)x.b[2] << 16)
         | ((uint32_t)x.b[1] <<  8) |  (uint32_t)x.b[0]       ;
}

constexpr uint64_t uile64_to_uint(uile64_t x)
{
  return   ((uint64_t)x.b[7] << 56) | ((uint64_t)x.b[6] << 48)
         | ((uint64_t)x.b[5] << 40) | ((uint64_t)x.b[4] << 32)
         | ((uint32_t)x.b[3] << 24) | ((uint32_t)x.b[2] << 16)
         | ((uint32_t)x.b[1] <<  8) |  (uint32_t)x.b[0]       ;
}

constexpr uint64_t uile128_clamp_to_uint64(uile128_t x)
{
  return (!uile64_to_uint(x.hi) ? uile64_to_uint(x.lo) : ~(uint64_t)0);
}

// uint*_t -> uile*_t

constexpr uile16_t uint_to_uile16(uint16_t x)
{
  return uile16_t{(uint8_t)x, (uint8_t)(x >> 8)};
}

constexpr uile32_t uint_to_uile32(uint32_t x)
{
  return uile32_t{(uint8_t) x       , (uint8_t)(x >>  8),
                  (uint8_t)(x >> 16), (uint8_t)(x >> 24) };
}

constexpr uile64_t uint_to_uile64(uint64_t x)
{
  return uile64_t{(uint8_t) x       , (uint8_t)(x >>  8),
                  (uint8_t)(x >> 16), (uint8_t)(x >> 24),
                  (uint8_t)(x >> 32), (uint8_t)(x >> 40),
                  (uint8_t)(x >> 48), (uint8_t)(x >> 56) };
}

constexpr uile128_t uint64_to_uile128(uint64_t x)
{
  return uile128_t{uint_to_uile64(x), {}};
}

constexpr uile128_t uint64_hilo_to_uile128(uint64_t hi, uint64_t lo)
{
  return uile128_t{uint_to_uile64(lo), uint_to_uile64(hi)};
}

// uibe*_t -> uint*_t

constexpr uint16_t uibe16_to_uint(uibe16_t x)
{
  return (x.b[0] << 8) | x.b[1];
}

constexpr uint32_t uibe32_to_uint(uibe32_t x)
{
  return   ((uint32_t)x.b[0] << 24) | ((uint32_t)x.b[1] << 16)
         | ((uint32_t)x.b[2] <<  8) |  (uint32_t)x.b[3]       ;
}

constexpr uint64_t uibe64_to_uint(uibe64_t x)
{
  return   ((uint64_t)x.b[0] << 56) | ((uint64_t)x.b[1] << 48)
         | ((uint64_t)x.b[2] << 40) | ((uint64_t)x.b[3] << 32)
         | ((uint32_t)x.b[4] << 24) | ((uint32_t)x.b[5] << 16)
         | ((uint32_t)x.b[6] <<  8) |  (uint32_t)x.b[7]       ;
}

// uint*_t -> uibe*_t

constexpr uibe16_t uint_to_uibe16(uint16_t x)
{
  return uibe16_t{(uint8_t)(x >> 8), (uint8_t)x};
}

constexpr uibe32_t uint_to_uibe32(uint32_t x)
{
  return uibe32_t{(uint8_t)(x >> 24), (uint8_t)(x >> 16),
                  (uint8_t)(x >>  8), (uint8_t) x        };
}

constexpr uibe64_t uint_to_uibe64(uint64_t x)
{
  return uibe64_t{(uint8_t)(x >> 56), (uint8_t)(x >> 48),
                  (uint8_t)(x >> 40), (uint8_t)(x >> 32),
                  (uint8_t)(x >> 24), (uint8_t)(x >> 16),
                  (uint8_t)(x >>  8), (uint8_t) x        };
}

// Compile-time checks
SMARTMON_STATIC_ASSERT(byteswap_uint16(0x1234) == 0x3412);
SMARTMON_STATIC_ASSERT(byteswap_uint32(0x12345678) == 0x78563412);
SMARTMON_STATIC_ASSERT(byteswap_uint64(0x123456789abcdef1) == 0xf1debc9a78563412);
SMARTMON_STATIC_ASSERT(uile16_to_uint(uile16_t{{0x34,0x12}}) == 0x1234);
SMARTMON_STATIC_ASSERT(uile16_to_uint(uint_to_uile16(0x1234)) == 0x1234);
SMARTMON_STATIC_ASSERT(uile32_to_uint(uile32_t{{0x78,0x56,0x34,0x12}}) == 0x12345678);
SMARTMON_STATIC_ASSERT(uile32_to_uint(uint_to_uile32(0x12345678)) == 0x12345678);
SMARTMON_STATIC_ASSERT(uile64_to_uint(uile64_t{{0xf1,0xde,0xbc,0x9a,0x78,0x56,0x34,0x12}}) == 0x123456789abcdef1);
SMARTMON_STATIC_ASSERT(uile64_to_uint(uint_to_uile64(0x123456789abcdef1)) == 0x123456789abcdef1);
SMARTMON_STATIC_ASSERT(uibe16_to_uint(uibe16_t{{0x12,0x34}}) == 0x1234);
SMARTMON_STATIC_ASSERT(uibe16_to_uint(uint_to_uibe16(0x1234)) == 0x1234);
SMARTMON_STATIC_ASSERT(uibe32_to_uint(uibe32_t{{0x12,0x34,0x56,0x78}}) == 0x12345678);
SMARTMON_STATIC_ASSERT(uibe32_to_uint(uint_to_uibe32(0x12345678)) == 0x12345678);
SMARTMON_STATIC_ASSERT(uibe64_to_uint(uibe64_t{{0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf1}}) == 0x123456789abcdef1);
SMARTMON_STATIC_ASSERT(uibe64_to_uint(uint_to_uibe64(0x123456789abcdef1)) == 0x123456789abcdef1);
SMARTMON_STATIC_ASSERT(uile128_clamp_to_uint64(uint64_to_uile128(0x123456789abcdef1)) == 0x123456789abcdef1);
SMARTMON_STATIC_ASSERT(uile128_clamp_to_uint64(uile128_t{{},{1}}) == 0xffffffffffffffff);

} // namespace smartmon

#endif // SMARTMON_BYTEORDER_H
