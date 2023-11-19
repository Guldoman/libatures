if freetype_dep.found() and not get_option('no_tests')
  fs = import('fs')
  fonts_deps = fs.copyfile('fonts/JetBrainsMono-Regular.ttf', 'JetBrainsMono-Regular.ttf')

  test_chain_generation = executable('test_chain_generation', 'test_chain_generation.c',
    include_directories: include_directories('../src'),
    dependencies: [freetype_dep],
    link_with: liblib,
    build_by_default: false,
  )

  test_substitutions = executable('test_substitutions', 'test_substitutions.c',
    include_directories: include_directories('../src'),
    dependencies: [freetype_dep],
    link_with: liblib,
    build_by_default: false,
  )

  test('Test chain generation', test_chain_generation,
    protocol: 'tap'
  )

  test('Test substitutions', test_substitutions,
    protocol: 'tap'
  )
else
  warning('Testing disabled because freetype wasn\'t found, was disabled, or testing was disabled')
endif
