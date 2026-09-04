project('gstklvplugin', 'c',
  version : '1.0.1',
  license : 'AGPL-3.0-only',
  meson_version : '>= 0.62',
  default_options : [
    'warning_level=2',
    'c_std=gnu11',
  ]
)

gst_req = '>= 1.20'
tests_opt = get_option('tests')

gst_dep      = dependency('gstreamer-1.0',      version : gst_req)
gst_base_dep = dependency('gstreamer-base-1.0', version : gst_req)
glib_dep     = dependency('glib-2.0')
cc           = meson.get_compiler('c')
m_dep        = cc.find_library('m', required : true)

# Optional: gst-check for unit tests
gst_check_dep = dependency('gstreamer-check-1.0', version : gst_req, required : tests_opt)

datadir = get_option('datadir')

install_data('data/stanag4609_tags.ini',
  install_dir : datadir / 'gstklvplugin'
)

install_data(
  ['LICENSE', 'README.md', 'CHANGELOG.md'],
  install_dir : datadir / 'doc' / 'gstklvplugin'
)

subdir('src')

dev_env = environment()
dev_env.prepend('GST_PLUGIN_PATH', meson.project_build_root() / 'src')
dev_env.set('KLV_TAGS_INI', meson.project_source_root() / 'data' / 'stanag4609_tags.ini')
meson.add_devenv(dev_env)

if not tests_opt.disabled() and gst_check_dep.found()
  subdir('tests/check')
  subdir('tests/smoke')
endif
