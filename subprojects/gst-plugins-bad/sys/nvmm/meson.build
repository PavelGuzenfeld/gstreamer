nvmm_sources = [
  'gstnvmmallocator.c',
  'gstnvmmconvert.c',
  'plugin.c',
]

doc_sources = []
foreach s : nvmm_sources
  doc_sources += meson.current_source_dir() / s
endforeach

plugin_sources += {
  'nvmm': pathsep.join(doc_sources)
}

if get_option('nvmm').disabled()
  subdir_done()
endif

nvmm_incdirs = [configinc]
nvmm_args = ['-DGST_USE_UNSTABLE_API']
nvmm_deps = [gstbase_dep, gstvideo_dep, gstallocators_dep]

# Check for NvBufSurface on Jetson
nvbuf_surface_dep = dependency('nvbufsurface', required : false)
have_nvbufsurface = false

if not nvbuf_surface_dep.found()
  nvbuf_surface_lib = cc.find_library('nvbufsurface',
    dirs : ['/usr/lib/aarch64-linux-gnu/tegra', '/usr/lib/aarch64-linux-gnu'],
    required : false)

  if nvbuf_surface_lib.found()
    nvbuf_surface_inc = include_directories(
      '/usr/src/jetson_multimedia_api/include',
      is_system : true)
    nvbuf_surface_dep = declare_dependency(
      dependencies : nvbuf_surface_lib,
      include_directories : nvbuf_surface_inc)
    have_nvbufsurface = true
  endif
else
  have_nvbufsurface = true
endif

# Check for NvBufSurfTransform
nvbuf_transform_dep = dependency('nvbufsurftransform', required : false)
if not nvbuf_transform_dep.found()
  nvbuf_transform_lib = cc.find_library('nvbufsurftransform',
    dirs : ['/usr/lib/aarch64-linux-gnu/tegra', '/usr/lib/aarch64-linux-gnu'],
    required : false)
  if nvbuf_transform_lib.found()
    nvbuf_transform_dep = declare_dependency(
      dependencies : nvbuf_transform_lib)
  endif
endif

if have_nvbufsurface
  nvmm_deps += [nvbuf_surface_dep, nvbuf_transform_dep]
else
  # Build with mock/stub API for testing on non-Jetson hosts
  message('NvBufSurface not found — building nvmm plugin with stub API')
  nvmm_args += ['-DNVMM_MOCK_API']
endif

gstnvmm = library('gstnvmm',
  nvmm_sources,
  c_args : gst_plugins_bad_args + nvmm_args,
  include_directories : nvmm_incdirs,
  dependencies : nvmm_deps,
  install : true,
  install_dir : plugins_install_dir,
)

plugins += [gstnvmm]
