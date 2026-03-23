/**
 * @file src/ts/ts_crc32.c
 * @brief CRC-32/MPEG-2 implementation for PSI sections.
 * @ingroup gstklv_internal_ts
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 */

#include "gstklv/internal/ts_crc32.h"

static const guint32 TS_CRC32_POLY = 0x04C11DB7;

guint32
ts_crc32_mpeg2(const guint8 *data, gsize len)
{
  guint32 crc = 0xFFFFFFFF;
  for (gsize i = 0; i < len; i++) {
    crc ^= ((guint32)data[i]) << 24;
    for (gint bit = 0; bit < 8; bit++) {
      if (crc & 0x80000000) {
        crc = (crc << 1) ^ TS_CRC32_POLY;
      }
      else {
        crc <<= 1;
      }
    }
  }
  return crc;
}
