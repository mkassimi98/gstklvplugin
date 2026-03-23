/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/internal/ts_psi.h
 * @brief PAT/PMT parsing and PSI helpers.
 * @ingroup gstklv_internal_ts
 *
 * @section ts_psi_api API Summary
 *
 * | Function | Purpose |
 * | --- | --- |
 * | `ts_extract_section_from_packet` | Extract PSI section bytes |
 * | `ts_parse_pat_section` | Parse PAT and return PMT PID |
 * | `ts_parse_descriptors` | Parse descriptor list |
 * | `ts_parse_pmt_section` | Parse PMT into structured data |
 * | `ts_build_pmt_section` | Build PMT with CRC-32 |
 *
 * @section ts_psi_structs Structures
 *
 * - `TsDescriptor`: tag + payload bytes
 * - `TsStreamInfo`: stream type, PID, and descriptors
 * - `TsPmtInfo`: PMT header info and stream list
 *
 * @section ts_psi_flow PSI Flow
 *
 * @dot
 * digraph ts_psi_flow {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *   pat [label="PAT"];
 *   pmt [label="PMT"];
 *   parse [label="ts_parse_pmt_section"];
 *   build [label="ts_build_pmt_section"];
 *   pat -> pmt;
 *   pmt -> parse;
 *   parse -> build;
 * }
 * @enddot
 */
#ifndef GSTKLV_TS_PSI_H
#define GSTKLV_TS_PSI_H

#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
 * @brief MPEG-TS descriptor (tag + payload).
 */
  typedef struct
  {
    guint8 tag;          /**< Descriptor tag. */
    GByteArray *payload; /**< Descriptor payload bytes (owned). */
  } TsDescriptor;

  /**
 * @brief PMT ES entry.
 */
  typedef struct
  {
    guint8 stream_type;     /**< Stream type. */
    guint16 pid;            /**< Elementary PID. */
    GPtrArray *descriptors; /**< ES descriptors (of TsDescriptor*, owned). */
  } TsStreamInfo;

  /**
 * @brief Parsed PMT information.
 */
  typedef struct
  {
    guint16 program_number;     /**< Program number. */
    guint8 version_byte;        /**< Version byte. */
    guint8 section_number;      /**< Section number. */
    guint8 last_section_number; /**< Last section number. */
    guint16 pcr_pid;            /**< PCR PID. */
    GByteArray *program_info;   /**< Program descriptors (owned). */
    GPtrArray *streams;         /**< ES stream list (of TsStreamInfo*, owned). */
  } TsPmtInfo;

  /**
 * @brief Free a TsDescriptor instance (GDestroyNotify compatible).
 */
  void ts_descriptor_free(gpointer data);

  /**
 * @brief Free a TsStreamInfo instance (GDestroyNotify compatible).
 */
  void ts_stream_info_free(gpointer data);

  /**
 * @brief Initialise a TsPmtInfo structure (allocates owned fields).
 */
  void ts_pmt_info_init(TsPmtInfo *pmt);

  /**
 * @brief Release all resources owned by a TsPmtInfo structure.
 */
  void ts_pmt_info_clear(TsPmtInfo *pmt);

  /**
 * @brief Extract a PSI section from a TS packet.
 */
  gboolean ts_extract_section_from_packet(const guint8 *pkt,
                                          gsize pkt_len,
                                          guint8 expected_table,
                                          const guint8 **out_section,
                                          gsize *out_len);

  /**
 * @brief Parse a PAT section and extract the PMT PID.
 */
  gboolean ts_parse_pat_section(const guint8 *section, gsize len, guint16 *out_pmt_pid);

  /**
 * @brief Parse descriptor bytes into a GPtrArray of TsDescriptor*.
 * @return GPtrArray (caller must g_ptr_array_unref), or NULL on failure.
 */
  GPtrArray *ts_parse_descriptors(const guint8 *data, gsize len);

  /**
 * @brief Parse a PMT section into a structured object.
 */
  gboolean ts_parse_pmt_section(const guint8 *section, gsize len, TsPmtInfo *out_pmt);

  /**
 * @brief Build a PMT section with CRC-32/MPEG-2.
 * @return GByteArray (caller must g_byte_array_unref), or NULL on failure.
 */
  GByteArray *ts_build_pmt_section(const TsPmtInfo *pmt);

#ifdef __cplusplus
}
#endif

#endif /* GSTKLV_TS_PSI_H */
