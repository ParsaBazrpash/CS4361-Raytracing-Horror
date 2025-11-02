project('test-vpn', 'c',
  version: '0.1',
  default_options: ['c_std=c11', 'warning_level=3'])

cc = meson.get_compiler('c')

sources = ['src/main.c']

raylib  = dependency('raylib', required: true)

# Use OpenBLAS (exports cblas_*). Do NOT use 'blas' here.
openblas = dependency('openblas', required: true)

# LAPACKE optional (you’re not calling it yet). Keep if you plan to use it.
lapacke = dependency('lapacke', required: false)
if not lapacke.found()
  lapacke = cc.find_library('lapacke', required: false)
endif

executable(
  'main',
  sources,
  dependencies: [raylib, openblas, lapacke],
  link_args: ['-lm']
)
