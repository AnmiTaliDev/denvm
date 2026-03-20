lib_sources = files(
  '../src/loader.c',
  '../src/decode.c',
  '../src/xref.c',
  '../src/output.c',
)

test_incdir = include_directories('../include')

test_decode = executable('test_decode',
  files('test_decode.c') + lib_sources,
  include_directories : test_incdir,
)

test_xref = executable('test_xref',
  files('test_xref.c') + lib_sources,
  include_directories : test_incdir,
)

test_loader = executable('test_loader',
  files('test_loader.c') + lib_sources,
  include_directories : test_incdir,
)

test('decode', test_decode)
test('xref',   test_xref)
test('loader', test_loader)
