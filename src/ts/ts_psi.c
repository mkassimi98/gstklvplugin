/**
 * @file src/ts/ts_psi.c
 * @brief PAT/PMT parsing and PSI helpers implementation.
 * @ingroup gstklv_internal_ts
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 */

#include "gstklv/internal/ts_psi.h"
#include "gstklv/internal/ts_crc32.h"

static const guint TS_PACKET_SIZE = 188;

void
ts_descriptor_free(gpointer data)
{
  TsDescriptor *d = (TsDescriptor *)data;
  if (!d)
    return;
  if (d->payload)
    g_byte_array_unref(d->payload);
  g_free(d);
}

void
ts_stream_info_free(gpointer data)
{
  TsStreamInfo *s = (TsStreamInfo *)data;
  if (!s)
    return;
  if (s->descriptors)
    g_ptr_array_unref(s->descriptors);
  g_free(s);
}

void
ts_pmt_info_init(TsPmtInfo *pmt)
{
  if (!pmt)
    return;
  pmt->program_number = 0;
  pmt->version_byte = 0;
  pmt->section_number = 0;
  pmt->last_section_number = 0;
  pmt->pcr_pid = 0;
  pmt->program_info = g_byte_array_new();
  pmt->streams = g_ptr_array_new_with_free_func(ts_stream_info_free);
}

void
ts_pmt_info_clear(TsPmtInfo *pmt)
{
  if (!pmt)
    return;
  if (pmt->program_info) {
    g_byte_array_unref(pmt->program_info);
    pmt->program_info = NULL;
  }
  if (pmt->streams) {
    g_ptr_array_unref(pmt->streams);
    pmt->streams = NULL;
  }
}

gboolean
ts_extract_section_from_packet(const guint8 *pkt,
                               gsize pkt_len,
                               guint8 expected_table,
                               const guint8 **out_section,
                               gsize *out_len)
{
  if (pkt_len < TS_PACKET_SIZE || pkt[0] != 0x47)
    return FALSE;

  guint8 afc = (pkt[3] >> 4) & 0x03;
  guint offset = 4;
  if (afc == 0 || afc == 2)
    return FALSE;

  if (afc == 3) {
    if (offset >= pkt_len)
      return FALSE;
    guint8 afl = pkt[offset];
    offset += 1 + afl;
  }

  if (offset + 1 >= pkt_len)
    return FALSE;

  if (!(pkt[1] & 0x40))
    return FALSE;

  guint8 pointer = pkt[offset];
  offset += 1 + pointer;
  if (offset + 3 > pkt_len)
    return FALSE;

  if (pkt[offset] != expected_table)
    return FALSE;

  guint16 section_length = ((pkt[offset + 1] & 0x0F) << 8) | pkt[offset + 2];
  gsize total_len = 3 + section_length;
  if (offset + total_len > pkt_len)
    return FALSE;

  *out_section = pkt + offset;
  *out_len = total_len;
  return TRUE;
}

gboolean
ts_parse_pat_section(const guint8 *section, gsize len, guint16 *out_pmt_pid)
{
  if (len < 12 || section[0] != 0x00)
    return FALSE;

  guint16 section_length = ((section[1] & 0x0F) << 8) | section[2];
  gsize total_len = 3 + section_length;
  if (total_len > len || section_length < 4)
    return FALSE;

  /* Validate CRC-32/MPEG-2: covers all bytes except the trailing 4 CRC bytes */
  gsize crc_input_len = total_len - 4;
  guint32 received_crc =
    ((guint32)section[crc_input_len] << 24) | ((guint32)section[crc_input_len + 1] << 16) |
    ((guint32)section[crc_input_len + 2] << 8) | (guint32)section[crc_input_len + 3];
  guint32 computed_crc = ts_crc32_mpeg2(section, crc_input_len);
  if (computed_crc != received_crc)
    return FALSE;

  gsize end = crc_input_len;

  gsize pos = 8;
  while (pos + 4 <= end) {
    guint16 program_number = (section[pos] << 8) | section[pos + 1];
    guint16 pid = ((section[pos + 2] & 0x1F) << 8) | section[pos + 3];
    if (program_number != 0) {
      *out_pmt_pid = pid;
      return TRUE;
    }
    pos += 4;
  }

  return FALSE;
}

GPtrArray *
ts_parse_descriptors(const guint8 *data, gsize len)
{
  GPtrArray *out = g_ptr_array_new_with_free_func(ts_descriptor_free);
  gsize i = 0;
  while (i + 1 < len) {
    guint8 tag = data[i];
    guint8 dlen = data[i + 1];
    if (i + 2 + dlen > len)
      break;
    TsDescriptor *d = g_new0(TsDescriptor, 1);
    d->tag = tag;
    d->payload = g_byte_array_new();
    g_byte_array_append(d->payload, data + i + 2, dlen);
    g_ptr_array_add(out, d);
    i += 2 + dlen;
  }
  return out;
}

gboolean
ts_parse_pmt_section(const guint8 *section, gsize len, TsPmtInfo *out_pmt)
{
  if (len < 16 || section[0] != 0x02)
    return FALSE;

  guint16 section_length = ((section[1] & 0x0F) << 8) | section[2];
  gsize total_len = 3 + section_length;
  if (total_len > len || section_length < 4)
    return FALSE;

  /* Validate CRC-32/MPEG-2: covers all bytes except the trailing 4 CRC bytes */
  gsize crc_input_len = total_len - 4;
  guint32 received_crc =
    ((guint32)section[crc_input_len] << 24) | ((guint32)section[crc_input_len + 1] << 16) |
    ((guint32)section[crc_input_len + 2] << 8) | (guint32)section[crc_input_len + 3];
  guint32 computed_crc = ts_crc32_mpeg2(section, crc_input_len);
  if (computed_crc != received_crc)
    return FALSE;

  out_pmt->program_number = (section[3] << 8) | section[4];
  out_pmt->version_byte = section[5];
  out_pmt->section_number = section[6];
  out_pmt->last_section_number = section[7];
  out_pmt->pcr_pid = ((section[8] & 0x1F) << 8) | section[9];
  guint16 program_info_length = ((section[10] & 0x0F) << 8) | section[11];

  gsize pos = 12;
  if (pos + program_info_length > crc_input_len)
    return FALSE;

  if (out_pmt->program_info)
    g_byte_array_set_size(out_pmt->program_info, 0);
  else
    out_pmt->program_info = g_byte_array_new();
  g_byte_array_append(out_pmt->program_info, section + pos, program_info_length);
  pos += program_info_length;

  if (out_pmt->streams)
    g_ptr_array_set_size(out_pmt->streams, 0);
  else
    out_pmt->streams = g_ptr_array_new_with_free_func(ts_stream_info_free);

  gsize end = crc_input_len;
  while (pos + 5 <= end) {
    TsStreamInfo *s = g_new0(TsStreamInfo, 1);
    s->stream_type = section[pos];
    s->pid = ((section[pos + 1] & 0x1F) << 8) | section[pos + 2];
    guint16 es_info_length = ((section[pos + 3] & 0x0F) << 8) | section[pos + 4];
    gsize desc_start = pos + 5;
    gsize desc_end = desc_start + es_info_length;
    if (desc_end > end) {
      ts_stream_info_free(s);
      break;
    }
    s->descriptors = ts_parse_descriptors(section + desc_start, es_info_length);
    g_ptr_array_add(out_pmt->streams, s);
    pos = desc_end;
  }

  return TRUE;
}

GByteArray *
ts_build_pmt_section(const TsPmtInfo *pmt)
{
  /* Build body first */
  GByteArray *body = g_byte_array_new();

  guint8 b;

  b = (pmt->program_number >> 8) & 0xFF;
  g_byte_array_append(body, &b, 1);
  b = pmt->program_number & 0xFF;
  g_byte_array_append(body, &b, 1);
  b = pmt->version_byte;
  g_byte_array_append(body, &b, 1);
  b = pmt->section_number;
  g_byte_array_append(body, &b, 1);
  b = pmt->last_section_number;
  g_byte_array_append(body, &b, 1);
  b = (guint8)(0xE0 | ((pmt->pcr_pid >> 8) & 0x1F));
  g_byte_array_append(body, &b, 1);
  b = pmt->pcr_pid & 0xFF;
  g_byte_array_append(body, &b, 1);

  guint16 program_info_length = pmt->program_info ? (guint16)pmt->program_info->len : 0;
  b = (guint8)(0xF0 | ((program_info_length >> 8) & 0x0F));
  g_byte_array_append(body, &b, 1);
  b = (guint8)(program_info_length & 0xFF);
  g_byte_array_append(body, &b, 1);
  if (pmt->program_info && pmt->program_info->len > 0)
    g_byte_array_append(body, pmt->program_info->data, pmt->program_info->len);

  guint num_streams = pmt->streams ? pmt->streams->len : 0;
  for (guint si = 0; si < num_streams; si++) {
    TsStreamInfo *s = (TsStreamInfo *)g_ptr_array_index(pmt->streams, si);

    /* Build descriptor bytes for this stream */
    GByteArray *desc_bytes = g_byte_array_new();
    guint num_descs = s->descriptors ? s->descriptors->len : 0;
    for (guint di = 0; di < num_descs; di++) {
      TsDescriptor *d = (TsDescriptor *)g_ptr_array_index(s->descriptors, di);
      guint8 dtag = d->tag;
      guint8 dlen = d->payload ? (guint8)d->payload->len : 0;
      g_byte_array_append(desc_bytes, &dtag, 1);
      g_byte_array_append(desc_bytes, &dlen, 1);
      if (d->payload && d->payload->len > 0)
        g_byte_array_append(desc_bytes, d->payload->data, d->payload->len);
    }

    guint16 es_info_length = (guint16)desc_bytes->len;
    b = s->stream_type;
    g_byte_array_append(body, &b, 1);
    b = (guint8)(0xE0 | ((s->pid >> 8) & 0x1F));
    g_byte_array_append(body, &b, 1);
    b = (guint8)(s->pid & 0xFF);
    g_byte_array_append(body, &b, 1);
    b = (guint8)(0xF0 | ((es_info_length >> 8) & 0x0F));
    g_byte_array_append(body, &b, 1);
    b = (guint8)(es_info_length & 0xFF);
    g_byte_array_append(body, &b, 1);
    if (desc_bytes->len > 0)
      g_byte_array_append(body, desc_bytes->data, desc_bytes->len);
    g_byte_array_unref(desc_bytes);
  }

  /* Prepend section header */
  guint16 section_length = (guint16)(body->len + 4);
  GByteArray *section = g_byte_array_new();
  b = 0x02;
  g_byte_array_append(section, &b, 1);
  b = (guint8)(0xB0 | ((section_length >> 8) & 0x0F));
  g_byte_array_append(section, &b, 1);
  b = (guint8)(section_length & 0xFF);
  g_byte_array_append(section, &b, 1);
  g_byte_array_append(section, body->data, body->len);
  g_byte_array_unref(body);

  /* Append CRC-32 */
  guint32 crc = ts_crc32_mpeg2(section->data, section->len);
  b = (guint8)((crc >> 24) & 0xFF);
  g_byte_array_append(section, &b, 1);
  b = (guint8)((crc >> 16) & 0xFF);
  g_byte_array_append(section, &b, 1);
  b = (guint8)((crc >> 8) & 0xFF);
  g_byte_array_append(section, &b, 1);
  b = (guint8)(crc & 0xFF);
  g_byte_array_append(section, &b, 1);

  return section;
}
