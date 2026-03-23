gst_app_dep = dependency('gstreamer-app-1.0', version : gst_req)

test_env = environment()
test_env.set('GST_PLUGIN_PATH', meson.project_build_root() / 'src')
test_env.set('KLV_TAGS_INI', meson.project_source_root() / 'data' / 'stanag4609_tags.ini')
test_env.set('GST_REGISTRY', meson.project_build_root() / 'tests' / 'check' / 'gst-registry.bin')

check_c_args = [
  '-DPACKAGE="gstklvplugin"',
  '-DGSTKLVPLUGIN_SOURCE_DIR="' + meson.project_source_root() + '"',
]

check_include = include_directories('../../include')
check_deps = [gst_dep, gst_base_dep, gst_check_dep, gst_app_dep, glib_dep, m_dep]

element_suites = [
  [
    'elements/test_klvmetaenc',
    [
      'elements/test_klvmetaenc.c',
      '../../src/klv/klv_json.c',
    ],
  ],
  [
    'elements/test_klvmetadec',
    [
      'elements/test_klvmetadec.c',
      '../../src/klv/klv_json.c',
    ],
  ],
  [
    'elements/test_klvframeinject',
    [
      'elements/test_klvframeinject.c',
      '../../src/klv/klv_json.c',
    ],
  ],
  [
    'elements/test_tspmtrewrite',
    [
      'elements/test_tspmtrewrite.c',
      '../../src/ts/ts_crc32.c',
      '../../src/ts/ts_psi.c',
    ],
  ],
]

utility_suites = [
  [
    'libs/test_klv_utils',
    [
      'libs/test_klv_utils.c',
      '../../src/klv/klv_ber.c',
      '../../src/klv/klv_checksum.c',
      '../../src/klv/klv_json.c',
      '../../src/klv/klv_scaling.c',
      '../../src/klv/klv_tag_defs.c',
    ],
  ],
  [
    'libs/test_ts_psi',
    [
      'libs/test_ts_psi.c',
      '../../src/ts/ts_crc32.c',
      '../../src/ts/ts_psi.c',
    ],
  ],
]

foreach suite : element_suites + utility_suites
  suite_name = suite[0]
  suite_sources = suite[1]

  exe = executable(
    suite_name.underscorify(),
    suite_sources,
    c_args : check_c_args,
    include_directories : check_include,
    dependencies : check_deps,
  )

  test(suite_name, exe, env : test_env, timeout : 60)
endforeach
