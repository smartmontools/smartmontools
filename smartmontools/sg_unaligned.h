#ifndef SG_UNALIGNED_H
#define SG_UNALIGNED_H

/*
 * Copyright (c) 2014-2018 Douglas Gilbert.
 * All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the BSD_LICENSE file.
 */

#include <stdbool.h>
#include <stdint.h>     /* for uint8_t and friends */
#include <string.h>     /* for memcpy */

#ifdef __cplusplus
extern "C" {
#endif

/* These inline functions convert integers (always unsigned) to byte streams
 * and vice versa. They have two goals:
 *   - change the byte ordering of integers between host order and big
 *     endian ("_be") or little endian ("_le")
 *   - copy the big or little endian byte stream so it complies with any
 *     alignment that host integers require
 *
 * Host integer to given endian byte stream is a "_put_" function taking
 * two arguments (integer and pointer to byte stream) returning void.
 * Given endian byte stream to host integer is a "_get_" function that takes
 * one argument and returns an integer of appropriate size (uint32_t for 24
 * bit operations, uint64_t for 48 bit operations).
 *
 * Big endian byte format "on the wire" is the default used by SCSI
 * standards (www.t10.org). Big endian is also the network byte order.
 * Little endian is used by ATA, PCI and NVMe.
 */

/* The generic form of these routines was borrowed from the Linux kernel,
 * via mhvtl. There is a specialised version of the main functions for
 * little endian or big endian provided that not-quite-standard defines for
 * endianness are available from the compiler and the <byteswap.h> header
 * (a GNU extension) has been detected by ./configure . To force the
 * generic version, use './configure --disable-fast-lebe ' . */

/* Note: Assumes that the source and destination locations do not overlap.
 * An example of overlapping source and destination:
 *     sg_put_unaligned_le64(j, ((uint8_t *)&j) + 1);
 * Best not to do things like that.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"     /* need this to see if HAVE_BYTESWAP_H */
#endif

#undef GOT_UNALIGNED_SPECIALS   /* just in case */

#if defined(__BYTE_ORDER__) && defined(HAVE_BYTESWAP_H) && \
    ! defined(IGNORE_FAST_LEBE)

#if defined(__LITTLE_ENDIAN__) || (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

#define GOT_UNALIGNED_SPECIALS 1

#include <byteswap.h>           /* for bswap_16(), bswap_32() and bswap_64() */

// #warning ">>>>>> Doing Little endian special unaligneds"

static inline uint16_t sg_get_unaligned_be16(const void *p)
{
        uint16_t u;

        memcpy(&u, p, 2);
        return bswap_16(u);
}

static inline uint32_t sg_get_unaligned_be32(const void *p)
{
        uint32_t u;

        memcpy(&u, p, 4);
        return bswap_32(u);
}

static inline uint64_t sg_get_unaligned_be64(const void *p)
{
        uint64_t u;

        memcpy(&u, p, 8);
        return bswap_64(u);
}

static inline void sg_put_unaligned_be16(uint16_t val, void *p)
{
        uint16_t u = bswap_16(val);

        memcpy(p, &u, 2);
}

static inline void sg_put_unaligned_be32(uint32_t val, void *p)
{
        uint32_t u = bswap_32(val);

        memcpy(p, &u, 4);
}

static inline void sg_put_unaligned_be64(uint64_t val, void *p)
{
        uint64_t u = bswap_64(val);

        memcpy(p, &u, 8);
}

static inline uint16_t sg_get_unaligned_le16(const void *p)
{
        uint16_t u;

        memcpy(&u, p, 2);
        return u;
}

static inline uint32_t sg_get_unaligned_le32(const void *p)
{
        uint32_t u;

        memcpy(&u, p, 4);
        return u;
}

static inline uint64_t sg_get_unaligned_le64(const void *p)
{
        uint64_t u;

        memcpy(&u, p, 8);
        return u;
}

static inline void sg_put_unaligned_le16(uint16_t val, void *p)
{
        memcpy(p, &val, 2);
}

static inline void sg_put_unaligned_le32(uint32_t val, void *p)
{
        memcpy(p, &val, 4);
}

static inline void sg_put_unaligned_le64(uint64_t val, void *p)
{
        memcpy(p, &val, 8);
}

#elif defined(__BIG_ENDIAN__) || (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)

#define GOT_UNALIGNED_SPECIALS 1

#include <byteswap.h>

// #warning ">>>>>> Doing BIG endian special unaligneds"

static inline uint16_t sg_get_unaligned_le16(const void *p)
{
        uint16_t u;

        memcpy(&u, p, 2);
        return bswap_16(u);
}

static inline uint32_t sg_get_unaligned_le32(const void *p)
{
        uint32_t u;

        memcpy(&u, p, 4);
        return bswap_32(u);
}

static inline uint64_t sg_get_unaligned_le64(const void *p)
{
        uint64_t u;

        memcpy(&u, p, 8);
        return bswap_64(u);
}

static inline void sg_put_unaligned_le16(uint16_t val, void *p)
{
        uint16_t u = bswap_16(val);

        memcpy(p, &u, 2);
}

static inline void sg_put_unaligned_le32(uint32_t val, void *p)
{
        uint32_t u = bswap_32(val);

        memcpy(p, &u, 4);
}

static inline void sg_put_unaligned_le64(uint64_t val, void *p)
{
        uint64_t u = bswap_64(val);

        memcpy(p, &u, 8);
}

static inline uint16_t sg_get_unaligned_be16(const void *p)
{
        uint16_t u;

        memcpy(&u, p, 2);
        return u;
}

static inline uint32_t sg_get_unaligned_be32(const void *p)
{
        uint32_t u;

        memcpy(&u, p, 4);
        return u;
}

static inline uint64_t sg_get_unaligned_be64(const void *p)
{
        uint64_t u;

        memcpy(&u, p, 8);
        return u;
}

static inline void sg_put_unaligned_be16(uint16_t val, void *p)
{
        memcpy(p, &val, 2);
}

static inline void sg_put_unaligned_be32(uint32_t val, void *p)
{
        memcpy(p, &val, 4);
}

static inline void sg_put_unaligned_be64(uint64_t val, void *p)
{
        memcpy(p, &val, 8);
}

#endif          /* __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__  */
#endif          /* #if defined __BYTE_ORDER__ && defined <byteswap.h> &&
                 *     ! defined IGNORE_FAST_LEBE */


#ifndef GOT_UNALIGNED_SPECIALS

/* Now we have no tricks left, so use the only way this can be done
 * correctly in C safely: lots of shifts. */

// #warning ">>>>>> Doing GENERIC unaligneds"

static inline uint16_t sg_get_unaligned_be16(const void *p)
{
        return ((const uint8_t *)p)[0] << 8 | ((const uint8_t *)p)[1];
}

static inline uint32_t sg_get_unaligned_be32(const void *p)
{
        return ((const uint8_t *)p)[0] << 24 | ((const uint8_t *)p)[1] << 16 |
                ((const uint8_t *)p)[2] << 8 | ((const uint8_t *)p)[3];
}

static inline uint64_t sg_get_unaligned_be64(const void *p)
{
        return (uint64_t)sg_get_unaligned_be32(p) << 32 |
               sg_get_unaligned_be32((const uint8_t *)p + 4);
}

static inline void sg_put_unaligned_be16(uint16_t val, void *p)
{
        ((uint8_t *)p)[0] = (uint8_t)(val >> 8);
        ((uint8_t *)p)[1] = (uint8_t)val;
}

static inline void sg_put_unaligned_be32(uint32_t val, void *p)
{
        sg_put_unaligned_be16(val >> 16, p);
        sg_put_unaligned_be16(val, (uint8_t *)p + 2);
}

static inline void sg_put_unaligned_be64(uint64_t val, void *p)
{
        sg_put_unaligned_be32(val >> 32, p);
        sg_put_unaligned_be32(val, (uint8_t *)p + 4);
}


static inline uint16_t sg_get_unaligned_le16(const void *p)
{
        return ((const uint8_t *)p)[1] << 8 | ((const uint8_t *)p)[0];
}

static inline uint32_t sg_get_unaligned_le32(const void *p)
{
        return ((const uint8_t *)p)[3] << 24 | ((const uint8_t *)p)[2] << 16 |
                ((const uint8_t *)p)[1] << 8 | ((const uint8_t *)p)[0];
}

static inline uint64_t sg_get_unaligned_le64(const void *p)
{
        return (uint64_t)sg_get_unaligned_le32((const uint8_t *)p + 4) << 32 |
               sg_get_unaligned_le32(p);
}

static inline void sg_put_unaligned_le16(uint16_t val, void *p)
{
        ((uint8_t *)p)[0] = val & 0xff;
        ((uint8_t *)p)[1] = val >> 8;
}

static inline void sg_put_unaligned_le32(uint32_t val, void *p)
{
        sg_put_unaligned_le16(val >> 16, (uint8_t *)p + 2);
        sg_put_unaligned_le16(val, p);
}

static inline void sg_put_unaligned_le64(uint64_t val, void *p)
{
        sg_put_unaligned_le32(val >> 32, (uint8_t *)p + 4);
        sg_put_unaligned_le32(val, p);
}

#endif          /* #ifndef GOT_UNALIGNED_SPECIALS */

/* Following are lesser used conversions that don't have specializations
 * for endianness; big endian first. In summary these are the 24, 48 bit and
 * given-length conversions plus the "nz" conditional put conversions. */

/* Now big endian, get 24+48 then put 24+48 */
static inline uint32_t sg_get_unaligned_be24(const void *p)
{
        return ((const uint8_t *)p)[0] << 16 | ((const uint8_t *)p)[1] << 8 |
               ((const uint8_t *)p)[2];
}

/* Assume 48 bit value placed in uint64_t */
static inline uint64_t sg_get_unaligned_be48(const void *p)
{
        return (uint64_t)sg_get_unaligned_be16(p) << 32 |
               sg_get_unaligned_be32((const uint8_t *)p + 2);
}

/* Returns 0 if 'num_bytes' is less than or equal to 0 or greater than
 * 8 (i.e. sizeof(uint64_t)). Else returns result in uint64_t which is
 * an 8 byte unsigned integer. */
static inline uint64_t sg_get_unaligned_be(int num_bytes, const void *p)
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

static inline void sg_put_unaligned_be24(uint32_t val, void *p)
{
        ((uint8_t *)p)[0] = (val >> 16) & 0xff;
        ((uint8_t *)p)[1] = (val >> 8) & 0xff;
        ((uint8_t *)p)[2] = val & 0xff;
}

/* Assume 48 bit value placed in uint64_t */
static inline void sg_put_unaligned_be48(uint64_t val, void *p)
{
        sg_put_unaligned_be16(val >> 32, p);
        sg_put_unaligned_be32(val, (uint8_t *)p + 2);
}

/* Now little endian, get 24+48 then put 24+48 */
static inline uint32_t sg_get_unaligned_le24(const void *p)
{
        return (uint32_t)sg_get_unaligned_le16(p) |
               ((const uint8_t *)p)[2] << 16;
}

/* Assume 48 bit value placed in uint64_t */
static inline uint64_t sg_get_unaligned_le48(const void *p)
{
        return (uint64_t)sg_get_unaligned_le16((const uint8_t *)p + 4) << 32 |
               sg_get_unaligned_le32(p);
}

static inline void sg_put_unaligned_le24(uint32_t val, void *p)
{
        ((uint8_t *)p)[2] = (val >> 16) & 0xff;
        ((uint8_t *)p)[1] = (val >> 8) & 0xff;
        ((uint8_t *)p)[0] = val & 0xff;
}

/* Assume 48 bit value placed in uint64_t */
static inline void sg_put_unaligned_le48(uint64_t val, void *p)
{
        ((uint8_t *)p)[5] = (val >> 40) & 0xff;
        ((uint8_t *)p)[4] = (val >> 32) & 0xff;
        ((uint8_t *)p)[3] = (val >> 24) & 0xff;
        ((uint8_t *)p)[2] = (val >> 16) & 0xff;
        ((uint8_t *)p)[1] = (val >> 8) & 0xff;
        ((uint8_t *)p)[0] = val & 0xff;
}

/* Returns 0 if 'num_bytes' is less than or equal to 0 or greater than
 * 8 (i.e. sizeof(uint64_t)). Else returns result in uint64_t which is
 * an 8 byte unsigned integer. */
static inline uint64_t sg_get_unaligned_le(int num_bytes, const void *p)
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

/* Since cdb and parameter blocks are often memset to zero before these
 * unaligned function partially fill them, then check for a val of zero
 * and ignore if it is with these variants. First big endian, then little */
static inline void sg_nz_put_unaligned_be16(uint16_t val, void *p)
{
        if (val)
                sg_put_unaligned_be16(val, p);
}

static inline void sg_nz_put_unaligned_be24(uint32_t val, void *p)
{
        if (val) {
                ((uint8_t *)p)[0] = (val >> 16) & 0xff;
                ((uint8_t *)p)[1] = (val >> 8) & 0xff;
                ((uint8_t *)p)[2] = val & 0xff;
        }
}

static inline void sg_nz_put_unaligned_be32(uint32_t val, void *p)
{
        if (val)
                sg_put_unaligned_be32(val, p);
}

static inline void sg_nz_put_unaligned_be64(uint64_t val, void *p)
{
        if (val)
            sg_put_unaligned_be64(val, p);
}

static inline void sg_nz_put_unaligned_le16(uint16_t val, void *p)
{
        if (val)
                sg_put_unaligned_le16(val, p);
}

static inline void sg_nz_put_unaligned_le24(uint32_t val, void *p)
{
        if (val) {
                ((uint8_t *)p)[2] = (val >> 16) & 0xff;
                ((uint8_t *)p)[1] = (val >> 8) & 0xff;
                ((uint8_t *)p)[0] = val & 0xff;
        }
}

static inline void sg_nz_put_unaligned_le32(uint32_t val, void *p)
{
        if (val)
                sg_put_unaligned_le32(val, p);
}

static inline void sg_nz_put_unaligned_le64(uint64_t val, void *p)
{
        if (val)
            sg_put_unaligned_le64(val, p);
}


#ifdef __cplusplus
}
#endif

#endif /* SG_UNALIGNED_H */
