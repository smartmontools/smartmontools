/*
 * unaligned.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2014-2017 Douglas Gilbert
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef UNALIGNED_H
#define UNALIGNED_H

#include <stdint.h>


/* Borrowed from the Linux kernel, via mhvtl */

/* In the first section below, functions that copy unsigned integers in a
 * computer's native format, to and from an unaligned big endian sequence of
 * bytes. Big endian byte format "on the wire" is the default used by SCSI
 * standards (www.t10.org). Big endian is also the network byte order. */

static inline uint16_t __get_unaligned_be16(const uint8_t *p)
{
        return p[0] << 8 | p[1];
}

static inline uint32_t __get_unaligned_be32(const uint8_t *p)
{
        return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

/* Assume 48 bit value placed in uint64_t */
static inline uint64_t __get_unaligned_be48(const uint8_t *p)
{
        return (uint64_t)__get_unaligned_be16(p) << 32 |
               __get_unaligned_be32(p + 2);
}

static inline uint64_t __get_unaligned_be64(const uint8_t *p)
{
        return (uint64_t)__get_unaligned_be32(p) << 32 |
               __get_unaligned_be32(p + 4);
}

static inline void __put_unaligned_be16(uint16_t val, uint8_t *p)
{
        *p++ = val >> 8;
        *p++ = val;
}

static inline void __put_unaligned_be32(uint32_t val, uint8_t *p)
{
        __put_unaligned_be16(val >> 16, p);
        __put_unaligned_be16(val, p + 2);
}

/* Assume 48 bit value placed in uint64_t */
static inline void __put_unaligned_be48(uint64_t val, uint8_t *p)
{
        __put_unaligned_be16(val >> 32, p);
        __put_unaligned_be32(val, p + 2);
}

static inline void __put_unaligned_be64(uint64_t val, uint8_t *p)
{
        __put_unaligned_be32(val >> 32, p);
        __put_unaligned_be32(val, p + 4);
}

static inline uint16_t get_unaligned_be16(const void *p)
{
        return __get_unaligned_be16((const uint8_t *)p);
}

static inline uint32_t get_unaligned_be24(const void *p)
{
        return ((const uint8_t *)p)[0] << 16 | ((const uint8_t *)p)[1] << 8 |
               ((const uint8_t *)p)[2];
}

static inline uint32_t get_unaligned_be32(const void *p)
{
        return __get_unaligned_be32((const uint8_t *)p);
}

/* Assume 48 bit value placed in uint64_t */
static inline uint64_t get_unaligned_be48(const void *p)
{
        return __get_unaligned_be48((const uint8_t *)p);
}

static inline uint64_t get_unaligned_be64(const void *p)
{
        return __get_unaligned_be64((const uint8_t *)p);
}

/* Returns 0 if 'num_bytes' is less than or equal to 0 or greater than
 * 8 (i.e. sizeof(uint64_t)). Else returns result in uint64_t which is
 * an 8 byte unsigned integer. */
static inline uint64_t get_unaligned_be(int num_bytes, const void *p)
{
        if ((num_bytes <= 0) || (num_bytes > (int)sizeof(uint64_t)))
                return 0;
        else {
                const uint8_t * xp = (const uint8_t *)p;
                uint64_t res = *xp;

                for (++xp; num_bytes > 1; ++xp, --num_bytes)
                        res = (res << 8) | *xp;
                return res;
        }
}

static inline void put_unaligned_be16(uint16_t val, void *p)
{
        __put_unaligned_be16(val, (uint8_t *)p);
}

static inline void put_unaligned_be24(uint32_t val, void *p)
{
        ((uint8_t *)p)[0] = (val >> 16) & 0xff;
        ((uint8_t *)p)[1] = (val >> 8) & 0xff;
        ((uint8_t *)p)[2] = val & 0xff;
}

static inline void put_unaligned_be32(uint32_t val, void *p)
{
        __put_unaligned_be32(val, (uint8_t *)p);
}

/* Assume 48 bit value placed in uint64_t */
static inline void put_unaligned_be48(uint64_t val, void *p)
{
        __put_unaligned_be48(val, (uint8_t *)p);
}

static inline void put_unaligned_be64(uint64_t val, void *p)
{
        __put_unaligned_be64(val, (uint8_t *)p);
}

/* Below are the little endian equivalents of the big endian functions
 * above. Little endian is used by ATA, PCI and NVMe.
 */

static inline uint16_t __get_unaligned_le16(const uint8_t *p)
{
        return p[1] << 8 | p[0];
}

static inline uint32_t __get_unaligned_le32(const uint8_t *p)
{
        return p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0];
}

static inline uint64_t __get_unaligned_le64(const uint8_t *p)
{
        return (uint64_t)__get_unaligned_le32(p + 4) << 32 |
               __get_unaligned_le32(p);
}

static inline void __put_unaligned_le16(uint16_t val, uint8_t *p)
{
        *p++ = val;
        *p++ = val >> 8;
}

static inline void __put_unaligned_le32(uint32_t val, uint8_t *p)
{
        __put_unaligned_le16(val >> 16, p + 2);
        __put_unaligned_le16(val, p);
}

static inline void __put_unaligned_le64(uint64_t val, uint8_t *p)
{
        __put_unaligned_le32(val >> 32, p + 4);
        __put_unaligned_le32(val, p);
}

static inline uint16_t get_unaligned_le16(const void *p)
{
        return __get_unaligned_le16((const uint8_t *)p);
}

static inline uint32_t get_unaligned_le24(const void *p)
{
        return (uint32_t)__get_unaligned_le16((const uint8_t *)p) |
               ((const uint8_t *)p)[2] << 16;
}

static inline uint32_t get_unaligned_le32(const void *p)
{
        return __get_unaligned_le32((const uint8_t *)p);
}

/* Assume 48 bit value placed in uint64_t */
static inline uint64_t get_unaligned_le48(const void *p)
{
        return (uint64_t)__get_unaligned_le16((const uint8_t *)p + 4) << 32 |
               __get_unaligned_le32((const uint8_t *)p);
}

static inline uint64_t get_unaligned_le64(const void *p)
{
        return __get_unaligned_le64((const uint8_t *)p);
}

/* Returns 0 if 'num_bytes' is less than or equal to 0 or greater than
 * 8 (i.e. sizeof(uint64_t)). Else returns result in uint64_t which is
 * an 8 byte unsigned integer. */
static inline uint64_t get_unaligned_le(int num_bytes, const void *p)
{
        if ((num_bytes <= 0) || (num_bytes > (int)sizeof(uint64_t)))
                return 0;
        else {
                const uint8_t * xp = (const uint8_t *)p + (num_bytes - 1);
                uint64_t res = *xp;

                for (--xp; num_bytes > 1; --xp, --num_bytes)
                        res = (res << 8) | *xp;
                return res;
        }
}

static inline void put_unaligned_le16(uint16_t val, void *p)
{
        __put_unaligned_le16(val, (uint8_t *)p);
}

static inline void put_unaligned_le24(uint32_t val, void *p)
{
        ((uint8_t *)p)[2] = (val >> 16) & 0xff;
        ((uint8_t *)p)[1] = (val >> 8) & 0xff;
        ((uint8_t *)p)[0] = val & 0xff;
}

static inline void put_unaligned_le32(uint32_t val, void *p)
{
        __put_unaligned_le32(val, (uint8_t *)p);
}

/* Assume 48 bit value placed in uint64_t */
static inline void put_unaligned_le48(uint64_t val, void *p)
{
        ((uint8_t *)p)[5] = (val >> 40) & 0xff;
        ((uint8_t *)p)[4] = (val >> 32) & 0xff;
        ((uint8_t *)p)[3] = (val >> 24) & 0xff;
        ((uint8_t *)p)[2] = (val >> 16) & 0xff;
        ((uint8_t *)p)[1] = (val >> 8) & 0xff;
        ((uint8_t *)p)[0] = val & 0xff;
}

static inline void put_unaligned_le64(uint64_t val, void *p)
{
        __put_unaligned_le64(val, (uint8_t *)p);
}

/* Returns true if one or more bytes starting at bp for length b_len are
 * all zero bytes. Returns false otherwise, including degenerate cases. */
static inline bool
all_zeros(const uint8_t * bp, int b_len)
{
    if ((NULL == bp) || (b_len <= 0))
        return false;
    for (--b_len; b_len >= 0; --b_len) {
        if (0x0 != bp[b_len])
            return false;
    }
    return true;
}

/* Returns true if one or more bytes starting at bp for length b_len are
 * all 0xff bytes. Returns false otherwise, including degenerate cases. */
static inline bool
all_ffs(const uint8_t * bp, int b_len)
{
    if ((NULL == bp) || (b_len <= 0))
        return false;
    for (--b_len; b_len >= 0; --b_len) {
        if (0xff != bp[b_len])
            return false;
    }
    return true;
}

#endif	/* UNALIGNED_H */
