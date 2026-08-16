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

display_logic_test = executable(
  'opentrackir-display-logic-test',
  'test-display-logic.c',
  '../src/opentrackir-display-logic.c',
  include_directories: include_directories('../src'),
  dependencies: [dependency('glib-2.0'), opentrackir_dep],
)

test('OpenTrackIR display logic', display_logic_test)

lifecycle_policy_test = executable(
  'opentrackir-lifecycle-policy-test',
  'test-lifecycle-policy.c',
  '../src/opentrackir-lifecycle-policy.c',
  include_directories: include_directories('../src'),
  dependencies: dependency('glib-2.0'),
)

test('OpenTrackIR lifecycle policy', lifecycle_policy_test)

status_notifier_menu_test = executable(
  'opentrackir-status-notifier-menu-test',
  'test-status-notifier-menu.c',
  '../src/opentrackir-status-notifier-menu.c',
  include_directories: include_directories('../src'),
  dependencies: dependency('gio-2.0'),
)

test('OpenTrackIR status notifier menu', status_notifier_menu_test)

uinput_policy_test = executable(
  'opentrackir-uinput-policy-test',
  'test-uinput-policy.c',
  '../src/opentrackir-uinput-policy.c',
  include_directories: include_directories('../src'),
  dependencies: [dependency('glib-2.0'), opentrackir_dep],
)

test('OpenTrackIR uinput policy', uinput_policy_test)

uinput_pointer_test = executable(
  'opentrackir-uinput-pointer-test',
  'test-uinput-pointer.c',
  '../src/opentrackir-uinput-pointer.c',
  '../src/opentrackir-uinput-policy.c',
  include_directories: include_directories('../src'),
  dependencies: [dependency('glib-2.0'), dependency('libevdev', version: '>= 1.10'), opentrackir_dep],
)

test('OpenTrackIR uinput pointer', uinput_pointer_test)
