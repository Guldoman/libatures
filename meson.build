project('libatures', 'c',
  version: '0.1',
  default_options: ['warning_level=3']
)

lib_args = []
if get_option('default_library') == 'shared'
  lib_args += '-DBUILDING_LIBATURES'
endif

if get_option('no_freetype')
  lib_args += '-DNO_FREETYPE'
  freetype_dep = dependency('', required: false)
else
  freetype_dep = dependency('freetype2', required: true)
endif

liblib = library('libatures',
  [
    'src/libatures.c',
    'src/gsub.c',
    'src/glypharray.c',
  ],
  install: true,
  c_args: lib_args,
  # c_shared_args: ['-DBUILDING_LIBATURES'], # Waiting for meson 1.3.0
  gnu_symbol_visibility: 'hidden',
  dependencies: [freetype_dep]
)

subdir('tests')

# Make this library usable as a Meson subproject.
libatures_dep = declare_dependency(
  link_with: liblib,
  include_directories: ['src']
)

# Make this library usable from the system's
# package manager.
install_headers('src/libatures.h')

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  name: 'libatures',
  filebase: 'libatures',
  description: 'Meson sample project.',
  subdirs: 'libatures',
  libraries: liblib,
  version: '0.1',
)
