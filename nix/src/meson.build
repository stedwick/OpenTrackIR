opentrackir_sources = [
  'main.c',
  'opentrackir-application.c',
  'opentrackir-display-logic.c',
  'opentrackir-lifecycle-policy.c',
  'opentrackir-mouse-worker.c',
  'opentrackir-session-controller.c',
  'opentrackir-session-state.c',
  'opentrackir-status-notifier.c',
  'opentrackir-uinput-policy.c',
  'opentrackir-uinput-pointer.c',
  'opentrackir-window.c',
]

opentrackir_deps = [
  dependency('gtk4', version: '>= 4.12'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('libevdev', version: '>= 1.10'),
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
