/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/internal/klv_ber.h
 * @brief BER length encoding/decoding helpers.
 * @ingroup gstklv_internal_klv
 *
 * @section klv_ber_api API Summary
 *
 * | Function | Purpose |
 * | --- | --- |
 * | `klv_ber_encode_length` | Encode a length into BER bytes |
 * | `klv_ber_decode_length` | Decode a BER length from a buffer |
 * | `klv_ber_append_length` | Append BER length to a `GByteArray` |
 *
 * @section klv_ber_notes Notes
 *
 * - Short form is used for lengths < 0x80.
 * - Long form supports larger local sets and byte payloads.
 */
#ifndef GSTKLV_KLV_BER_H
#define GSTKLV_KLV_BER_H

#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
 * @brief Encode a BER length into a buffer.
 * @param buffer Output buffer (at least 4 bytes).
 * @param length Length value to encode.
 * @return Number of bytes written.
 */
  gint klv_ber_encode_length(guint8 *buffer, gsize length);

  /**
 * @brief Decode a BER length from a buffer.
 * @param buffer Input buffer.
 * @param buffer_len Total length of buffer.
 * @param pos In/out cursor position.
 * @param length Output decoded length.
 * @return TRUE on success.
 */
  gboolean klv_ber_decode_length(const guint8 *buffer, gsize buffer_len, gsize *pos, gsize *length);

  /**
 * @brief Append a BER length to a GByteArray.
 * @param arr Output byte array.
 * @param length Length value to encode.
 */
  void klv_ber_append_length(GByteArray *arr, gsize length);

#ifdef __cplusplus
}
#endif

#endif /* GSTKLV_KLV_BER_H */
