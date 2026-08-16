include(GNUInstallDirs)

add_library(opentrackir SHARED
  src/tir5_protocol.c
  src/tir5_frame.c
  src/tir5_mouse.c
  src/tir5_device.c
  src/tir5_session.c
  src/tir5_tooling.c
)
if(WIN32)
  set_target_properties(opentrackir PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
find_package(Threads REQUIRED)
target_include_directories(opentrackir PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(opentrackir PUBLIC opentrackir_libusb Threads::Threads)
if(UNIX AND NOT APPLE)
  target_link_libraries(opentrackir PUBLIC m)
endif()

configure_file(
  opentrackir.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/opentrackir.pc
  @ONLY
)

install(TARGETS opentrackir
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
  DIRECTORY include/opentrackir
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/opentrackir.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

function(otir_copy_runtime_dlls target_name)
  if(WIN32)
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${target_name}>
        $<TARGET_FILE_DIR:${target_name}>
      COMMAND_EXPAND_LISTS
    )
  endif()
endfunction()

if(OPENTRACKIR_BUILD_TESTS)
  add_executable(test_tir5 tests/test_tir5.c)
  target_include_directories(test_tir5 PRIVATE src)
  target_link_libraries(test_tir5 PRIVATE opentrackir)
  otir_copy_runtime_dlls(test_tir5)
  add_test(NAME test_tir5 COMMAND test_tir5)
endif()

add_executable(opentrackir_stream_dump examples/stream_dump.c)
target_link_libraries(opentrackir_stream_dump PRIVATE opentrackir)
otir_copy_runtime_dlls(opentrackir_stream_dump)
