/**
 * @file src/plugins/plugin.c
 * @brief Plugin entry point and element registration.
 * @ingroup gstklv_plugins
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 */

#include <gst/gst.h>

#ifndef PACKAGE
#define PACKAGE "gstklvplugin"
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.0.1"
#endif

#include "gstklv/klvencode.h"
#include "gstklv/klvdecode.h"
#include "gstklv/klvframeinject.h"
#include "gstklv/tspmtrewrite.h"

/**
 * @brief Register all gstklvplugin elements with GStreamer.
 * @param plugin GStreamer plugin handle.
 * @return TRUE on success.
 */
static gboolean
plugin_init(GstPlugin *plugin)
{
  gboolean ret = TRUE;

  ret &= gst_element_register(plugin, "klvmetaenc", GST_RANK_NONE, GST_TYPE_KLV_ENCODE);
  ret &= gst_element_register(plugin, "klvmetadec", GST_RANK_NONE, GST_TYPE_KLV_DECODE);
  ret &= gst_element_register(plugin, "klvframeinject", GST_RANK_NONE, GST_TYPE_KLV_FRAME_INJECT);
  ret &= gst_element_register(plugin, "tspmtrewrite", GST_RANK_NONE, GST_TYPE_TS_PMT_REWRITE);

  return ret;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  klvplugin,
                  "KLV Plugin for SMPTE ST 336 and MISB ST 0601 Metadata",
                  plugin_init,
                  PACKAGE_VERSION,
                  "AGPL-3.0",
                  "gstklvplugin",
                  "https://github.com/mkassimi98/gstklvplugin")
