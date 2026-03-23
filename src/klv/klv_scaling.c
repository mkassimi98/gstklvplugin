/**
 * @file src/klv/klv_scaling.c
 * @brief Scaling and sign extension helpers for KLV fields.
 * @ingroup gstklv_internal_klv
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 */

#include "gstklv/internal/klv_scaling.h"

#include <math.h>

gint64
klv_sign_extend(guint64 value, gint bits)
{
  if (bits <= 0 || bits >= 64)
    return (gint64)value;

  guint64 mask = (1ULL << bits) - 1;
  guint64 sign_bit = 1ULL << (bits - 1);
  guint64 v = value & mask;
  if (v & sign_bit) {
    return (gint64)(v | (~mask));
  }
  return (gint64)v;
}

gdouble
klv_decode_with_range(guint64 raw, gdouble min, gdouble max, gboolean is_signed, gint bits)
{
  if (max <= min || bits <= 0)
    return min;

  if (is_signed) {
    gint64 smax = (bits == 64) ? G_MAXINT64 : (((gint64)1 << (bits - 1)) - 1);
    gint64 sval = klv_sign_extend(raw, bits);
    gdouble norm = ((gdouble)sval + (gdouble)smax) / (2.0 * (gdouble)smax);
    return min + norm * (max - min);
  }

  guint64 umax = (bits == 64) ? G_MAXUINT64 : (((guint64)1 << bits) - 1);
  gdouble norm = (gdouble)raw / (gdouble)umax;
  return min + norm * (max - min);
}
