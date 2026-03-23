# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com

set(GSTKLV_PLUGIN_SOURCES
  plugins/plugin.c
  plugins/klvencode.c
  plugins/klvdecode.c
  plugins/klvframeinject.c
  plugins/tspmtrewrite.c
)

set(GSTKLV_KLV_SOURCES
  klv/klv_ber.c
  klv/klv_checksum.c
  klv/klv_json.c
  klv/klv_scaling.c
  klv/klv_tag_defs.c
  klv/klv_ul.c
)

set(GSTKLV_TS_SOURCES
  ts/ts_crc32.c
  ts/ts_psi.c
)

add_library(gstklv_klv STATIC ${GSTKLV_KLV_SOURCES})
add_library(gstklv::klv ALIAS gstklv_klv)
target_compile_features(gstklv_klv PUBLIC c_std_11)
target_link_libraries(gstklv_klv
  PUBLIC
    gstklv::project_defaults
    gstklv::gstreamer
    "${GSTKLVPLUGIN_MATH_LIBRARY}"
)
set_target_properties(gstklv_klv PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(gstklv_ts STATIC ${GSTKLV_TS_SOURCES})
add_library(gstklv::ts ALIAS gstklv_ts)
target_compile_features(gstklv_ts PUBLIC c_std_11)
target_link_libraries(gstklv_ts
  PUBLIC
    gstklv::project_defaults
    gstklv::gstreamer
)
set_target_properties(gstklv_ts PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(gstklvplugin SHARED ${GSTKLV_PLUGIN_SOURCES})
target_compile_features(gstklvplugin PRIVATE c_std_11)
target_compile_definitions(gstklvplugin
  PRIVATE
    KLV_TAGS_DEFAULT_PATH=\"${CMAKE_INSTALL_FULL_DATADIR}/gstklvplugin/stanag4609_tags.ini\"
)
target_link_libraries(gstklvplugin
  PRIVATE
    gstklv::project_defaults
    gstklv::gstreamer
    gstklv::klv
    gstklv::ts
)
set_target_properties(gstklvplugin
  PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    PREFIX ""
    SUFFIX ".so"
)

install(
  TARGETS gstklvplugin
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/gstreamer-1.0"
)
