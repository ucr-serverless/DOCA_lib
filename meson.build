project('DOCA_lib', 'c',
  default_options: ['warning_level=3', 'optimization=3']
)
#
add_project_arguments([ '-Wno-unused-parameter', '-Wno-sign-compare', '-Wno-pointer-arith',  '-Wno-conversion', '-Wno-pedantic', '-D DOCA_ALLOW_EXPERIMENTAL_API' ], language: ['c', 'cpp'])

root_dir = meson.project_source_root()

# Compiler and linker flags
#cflags = ['-Wall', '-Werror', '-O3', '-Wno-unused-parameter', '-Wno-sign-compare', '-Wno-pointer-arith',  '-Wno-conversion']
cflags = []

doca_dep = dependency('doca-common', required: true)
doca_argp_dep = dependency('doca-argp', required: true)
doca_rdma_dep = dependency('doca-rdma', required: true)

doca_inc_dir = '/opt/mellanox/doca/include'

c_compiler = meson.get_compiler('c')
if c_compiler.has_header_symbol(doca_inc_dir + '/doca_build_config.h', 'DOCA_ARCH_DPU', dependencies: doca_dep)
    is_dpu = true
else
    is_dpu = false
endif
is_host = not is_dpu
message('DOCA is found')
if is_dpu
  message('We are on DPU')
else
  message('We are on Host')
endif

# Source files
src_files = files(
  'src/common_doca.c',
  'src/rdma_common_doca.c',
  'utils/sock_utils.c',
  'utils/log.c',

  )
inc_dirs = [ 'include', 'utils' ]
doca_inc = [ doca_inc_dir ]

# Programs
bin_programs = [
  ['rdma_send', ['example/rdma_multi_conn_send_main.c', 'example/rdma_multi_conn_send_sample.c']],
  ['rdma_receive', ['example/rdma_multi_conn_receive_main.c', 'example/rdma_multi_conn_receive_sample.c']],
]

libDOCA_lib = static_library(
  'DOCA_lib',  # Name of the library
  src_files,     # Source files
  include_directories: inc_dirs + doca_inc,  # Include root directory
  c_args: ['-Wall', '-Werror'],       # Additional compile flags
  dependencies: [],
  install: true                       # Whether to install the library
)

copy_lib = custom_target(
    'copy_static_library',
    output: 'libDOCA_lib',        # Output filename in the root directory
    input: libDOCA_lib,                # The built static library
    command: ['cp', '@INPUT@', root_dir],
    depends: [libDOCA_lib],              # Ensure this depends on the static library
    build_by_default: true           # Run during the default build process
)


foreach prog : bin_programs
  executable(prog[0],
    sources: prog[1] + src_files,
    include_directories: inc_dirs + doca_inc,
    c_args: cflags,
    dependencies: [doca_dep, doca_argp_dep, doca_rdma_dep],
    install: true
  )
endforeach

