/**
 * @file src/klv/klv_checksum.c
 * @brief BCC-16 checksum implementation for KLV local sets.
 * @ingroup gstklv_internal_klv
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 */

#include "gstklv/internal/klv_checksum.h"

guint16
klv_bcc_16(const guint8 *data, gsize len)
{
  guint32 sum = 0;
  gsize i = 0;
  while (i + 1 < len) {
    guint16 word = (guint16)((data[i] << 8) | data[i + 1]);
    sum = (sum + word) & 0xFFFF;
    i += 2;
  }
  if (i < len) {
    guint16 word = (guint16)(data[i] << 8);
    sum = (sum + word) & 0xFFFF;
  }
  return (guint16)sum;
}
