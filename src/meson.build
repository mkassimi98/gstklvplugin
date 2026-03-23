plugin_sources = [
  'plugins/plugin.c',
  'plugins/klvencode.c',
  'plugins/klvdecode.c',
  'plugins/klvframeinject.c',
  'plugins/tspmtrewrite.c',
  'klv/klv_ber.c',
  'klv/klv_checksum.c',
  'klv/klv_json.c',
  'klv/klv_scaling.c',
  'klv/klv_tag_defs.c',
  'klv/klv_ul.c',
  'ts/ts_crc32.c',
  'ts/ts_psi.c',
]

shared_library('gstklvplugin',
  plugin_sources,
  c_args : [
    '-DPACKAGE="gstklvplugin"',
    '-DPACKAGE_VERSION="1.0.0"',
    '-DKLV_TAGS_DEFAULT_PATH="' + (get_option('prefix') / datadir / 'gstklvplugin' / 'stanag4609_tags.ini') + '"',
  ],
  include_directories : include_directories('../include'),
  dependencies : [gst_dep, gst_base_dep, glib_dep, m_dep],
  name_prefix : '',
  install : true,
  install_dir : get_option('libdir') / 'gstreamer-1.0',
)
