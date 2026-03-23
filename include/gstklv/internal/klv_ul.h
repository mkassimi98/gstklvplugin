/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/internal/klv_ul.h
 * @brief MISB ST 0601 UL constant.
 * @ingroup gstklv_internal_klv
 *
 * @section klv_ul_api API Summary
 *
 * | Constant | Purpose |
 * | --- | --- |
 * | `KLV_MISB_ST0601_UL` | MISB ST 0601 Local Set UL |
 */
#ifndef GSTKLV_KLV_UL_H
#define GSTKLV_KLV_UL_H

#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
 * @brief 16-byte Universal Label for MISB ST 0601 local set.
 */
  extern const guint8 KLV_MISB_ST0601_UL[16];

#ifdef __cplusplus
}
#endif

#endif /* GSTKLV_KLV_UL_H */
