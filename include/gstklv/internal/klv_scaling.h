/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/internal/klv_scaling.h
 * @brief Scaling helpers for KLV ranges and signed values.
 * @ingroup gstklv_internal_klv
 *
 * @section klv_scaling_api API Summary
 *
 * | Function | Purpose |
 * | --- | --- |
 * | `klv_decode_with_range` | Map raw integer to physical units |
 * | `klv_sign_extend` | Sign-extend a value to a signed integer |
 *
 * @section klv_scaling_notes Notes
 *
 * - `bits` should match the tag length in bits.
 * - For signed tags, the raw value is interpreted using two's complement.
 */
#ifndef GSTKLV_KLV_SCALING_H
#define GSTKLV_KLV_SCALING_H

#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
 * @brief Convert a raw integer to a physical value using a range.
 */
  gdouble
  klv_decode_with_range(guint64 raw, gdouble min, gdouble max, gboolean is_signed, gint bits);

  /**
 * @brief Sign-extend a value with a given bit width.
 */
  gint64 klv_sign_extend(guint64 value, gint bits);

#ifdef __cplusplus
}
#endif

#endif /* GSTKLV_KLV_SCALING_H */
