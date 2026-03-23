/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/internal/klv_checksum.h
 * @brief BCC-16 checksum helper for KLV local sets.
 * @ingroup gstklv_internal_klv
 *
 * @section klv_checksum_api API Summary
 *
 * | Function | Purpose |
 * | --- | --- |
 * | `klv_bcc_16` | Compute BCC-16 checksum over a buffer |
 *
 * @section klv_checksum_notes Notes
 *
 * - Used to populate tag 1 (Checksum) in ST 0601 Local Sets.
 * - Computed over the Local Set bytes excluding the checksum tag itself.
 */
#ifndef GSTKLV_KLV_CHECKSUM_H
#define GSTKLV_KLV_CHECKSUM_H

#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
 * @brief Compute BCC-16 checksum over a byte range.
 * @param data Input buffer.
 * @param len Buffer length.
 * @return 16-bit checksum.
 */
  guint16 klv_bcc_16(const guint8 *data, gsize len);

#ifdef __cplusplus
}
#endif

#endif /* GSTKLV_KLV_CHECKSUM_H */
