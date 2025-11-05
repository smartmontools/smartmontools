/*
 * byteorder.h - Types and functions for LE and BE integers
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2025 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SMARTMON_BYTEORDER_H
#define SMARTMON_BYTEORDER_H

#include <smartmon/smartmon_defs.h>

#include <stdint.h>

namespace smartmon {

// Unaligned Little Endian unsigned integers
struct uile16_t { uint8_t b[2]; };
struct uile32_t { uint8_t b[4]; };
struct uile64_t { uint8_t b[8]; };

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

} // namespace smartmon

#endif // SMARTMON_BYTEORDER_H
