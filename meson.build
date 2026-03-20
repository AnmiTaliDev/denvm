project('denvm', 'c',
  version : '1.0.0',
  license : 'Apache-2.0',
  default_options : [
    'c_std=c11',
    'warning_level=3',
    'werror=false',
  ],
)

cc = meson.get_compiler('c')

add_project_arguments(
  cc.get_supported_arguments([
    '-Wno-unused-parameter',
  ]),
  language : 'c',
)

incdir = include_directories('include')

sources = files(
  'src/main.c',
  'src/loader.c',
  'src/decode.c',
  'src/xref.c',
  'src/output.c',
)

executable('denvm',
  sources,
  include_directories : incdir,
  install : true,
)

subdir('tests')
