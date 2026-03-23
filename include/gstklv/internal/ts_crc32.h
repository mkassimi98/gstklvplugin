/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/internal/ts_crc32.h
 * @brief CRC-32/MPEG-2 helper for PSI sections.
 * @ingroup gstklv_internal_ts
 *
 * @section ts_crc32_api API Summary
 *
 * | Function | Purpose |
 * | --- | --- |
 * | `ts_crc32_mpeg2` | Compute CRC-32/MPEG-2 for PSI sections |
 */
#ifndef GSTKLV_TS_CRC32_H
#define GSTKLV_TS_CRC32_H

#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
 * @brief Compute CRC-32/MPEG-2 over a buffer.
 */
  guint32 ts_crc32_mpeg2(const guint8 *data, gsize len);

#ifdef __cplusplus
}
#endif

#endif /* GSTKLV_TS_CRC32_H */
