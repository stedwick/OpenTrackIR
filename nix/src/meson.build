opentrackir_sources = [
  'main.c',
  'opentrackir-application.c',
  'opentrackir-display-logic.c',
  'opentrackir-session-controller.c',
  'opentrackir-session-state.c',
  'opentrackir-window.c',
]

opentrackir_deps = [
  dependency('gtk4', version: '>= 4.12'),
  dependency('libadwaita-1', version: '>= 1.4'),
  opentrackir_dep,
]

opentrackir_sources += gnome.compile_resources('opentrackir-resources',
  'opentrackir.gresource.xml',
  c_name: 'opentrackir'
)

executable('opentrackir', opentrackir_sources,
  dependencies: opentrackir_deps,
       install: true,
)
