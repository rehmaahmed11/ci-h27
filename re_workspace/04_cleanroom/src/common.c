/*
 * cleanroom/common.c
 * ------------------
 * Shared primitive implementations (CRC32 + BE helpers) for the clean-room
 * firmware re-implementation.  Public-domain style implementation of the
 * reflected CRC-32 algorithm.
 */

#include "common.h"
#include <stddef.h>
#include <stdint.h>

uint32_t cleanroom_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc;
    cleanroom_crc32_init(&crc);
    cleanroom_crc32_update(&crc, data, len);
    return cleanroom_crc32_final(crc);
}

void cleanroom_crc32_init(uint32_t *crc)
{
    *crc = CLEANROOM_CRC32_INIT;
}

void cleanroom_crc32_update(uint32_t *crc, const uint8_t *data, size_t len)
{
    uint32_t c = *crc;
    size_t   i;
    unsigned int bit;

    for (i = 0; i < len; ++i) {
        c ^= data[i];
        for (bit = 0; bit < 8; ++bit) {
            if (c & 1u) {
                c = (c >> 1) ^ CLEANROOM_CRC32_POLY;
            } else {
                c = (c >> 1);
            }
        }
    }
    *crc = c;
}

uint32_t cleanroom_crc32_final(uint32_t crc)
{
    return crc ^ CLEANROOM_CRC32_XOROUT;
}

void cleanroom_be32_put(uint8_t out[4], uint32_t v)
{
    out[0] = (uint8_t)((v >> 24) & 0xFFu);
    out[1] = (uint8_t)((v >> 16) & 0xFFu);
    out[2] = (uint8_t)((v >>  8) & 0xFFu);
    out[3] = (uint8_t)( v        & 0xFFu);
}

uint32_t cleanroom_be32_get(const uint8_t in[4])
{
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] <<  8) |  ((uint32_t)in[3]);
}
