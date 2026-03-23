python3 = find_program('python3')
cmake = find_program('cmake', required : false)

smoke_env = environment()
smoke_env.set('GST_PLUGIN_PATH', meson.project_build_root() / 'src')
smoke_env.set('KLV_TAGS_INI', meson.project_source_root() / 'data' / 'stanag4609_tags.ini')
smoke_env.set('GST_REGISTRY', meson.project_build_root() / 'tests' / 'smoke' / 'gst-registry.bin')
smoke_env.set('GSTKLVPLUGIN_SOURCE_DIR', meson.project_source_root())
smoke_env.set('GSTKLVPLUGIN_BUILD_DIR', meson.project_build_root())
smoke_env.set('PYTHONDONTWRITEBYTECODE', '1')
smoke_env.set('PYTHONUNBUFFERED', '1')

smoke_tests = [
  ['smoke/ts_roundtrip_python', 'smoke_ts_roundtrip.py', 120],
  ['smoke/udp_loopback_python', 'smoke_udp_loopback.py', 120],
  ['smoke/staged_install_clean_registry', 'smoke_staged_install_clean_registry.py', 180],
]

if cmake.found()
  smoke_tests += [
    ['smoke/ts_roundtrip_cpp', 'smoke_ts_roundtrip_cpp.py', 240],
  ]
endif

foreach smoke : smoke_tests
  test(
    smoke[0],
    python3,
    args : [files(smoke[1])],
    env : smoke_env,
    timeout : smoke[2],
  )
endforeach
