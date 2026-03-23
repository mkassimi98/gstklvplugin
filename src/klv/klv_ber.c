/**
 * @file src/klv/klv_ber.c
 * @brief BER length encoding/decoding implementation.
 * @ingroup gstklv_internal_klv
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 */

#include "gstklv/internal/klv_ber.h"

gint
klv_ber_encode_length(guint8 *buffer, gsize length)
{
  if (length < 128) {
    buffer[0] = (guint8)length;
    return 1;
  }
  else if (length < 256) {
    buffer[0] = 0x81;
    buffer[1] = (guint8)length;
    return 2;
  }
  else if (length < 65536) {
    buffer[0] = 0x82;
    buffer[1] = (guint8)((length >> 8) & 0xFF);
    buffer[2] = (guint8)(length & 0xFF);
    return 3;
  }

  buffer[0] = 0x83;
  buffer[1] = (guint8)((length >> 16) & 0xFF);
  buffer[2] = (guint8)((length >> 8) & 0xFF);
  buffer[3] = (guint8)(length & 0xFF);
  return 4;
}

gboolean
klv_ber_decode_length(const guint8 *buffer, gsize buffer_len, gsize *pos, gsize *length)
{
  if (!buffer || !pos || !length)
    return FALSE;
  if (*pos >= buffer_len)
    return FALSE;

  guint8 len_byte = buffer[*pos];
  (*pos)++;

  if (len_byte < 128) {
    *length = len_byte;
    return TRUE;
  }

  gint num_octets = len_byte & 0x7F;
  if (*pos + num_octets > buffer_len)
    return FALSE;

  *length = 0;
  for (gint i = 0; i < num_octets; i++) {
    *length = (*length << 8) | buffer[*pos];
    (*pos)++;
  }

  return TRUE;
}

void
klv_ber_append_length(GByteArray *arr, gsize length)
{
  if (!arr)
    return;

  if (length < 0x80) {
    guint8 b = (guint8)length;
    g_byte_array_append(arr, &b, 1);
    return;
  }

  guint8 len_bytes[8];
  guint n = 0;
  gsize tmp = length;
  while (tmp > 0 && n < sizeof(len_bytes)) {
    len_bytes[n++] = (guint8)(tmp & 0xFF);
    tmp >>= 8;
  }
  guint8 hdr = (guint8)(0x80 | n);
  g_byte_array_append(arr, &hdr, 1);
  for (gint i = (gint)n - 1; i >= 0; i--) {
    g_byte_array_append(arr, &len_bytes[i], 1);
  }
}
