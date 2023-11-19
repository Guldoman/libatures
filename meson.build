project('libatures', 'c',
  version: '0.1',
  default_options: ['warning_level=3']
)

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_LIBATURES']

if get_option('no_freetype')
  lib_args += '-DNO_FREETYPE'
  freetype_dep = dependency('', required: false)
else
  freetype_dep = dependency('freetype2', required: true)
endif

shlib = library('libatures',
  [
    'src/libatures.c',
    'src/gsub.c',
    'src/glypharray.c',
  ],
  install: true,
  c_args: lib_args,
  gnu_symbol_visibility: 'hidden',
  dependencies: [freetype_dep]
)

subdir('tests')

# Make this library usable as a Meson subproject.
libatures_dep = declare_dependency(
  link_with: shlib,
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
  libraries: shlib,
  version: '0.1',
)
