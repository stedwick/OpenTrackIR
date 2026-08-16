core_link_test = executable(
  'opentrackir-core-link-test',
  'test-core-link.c',
  dependencies: opentrackir_dep,
)

test('OpenTrackIR core link', core_link_test)

session_state_test = executable(
  'opentrackir-session-state-test',
  'test-session-state.c',
  '../src/opentrackir-session-state.c',
  include_directories: include_directories('../src'),
  dependencies: [dependency('glib-2.0'), opentrackir_dep],
)

test('OpenTrackIR session state', session_state_test)
