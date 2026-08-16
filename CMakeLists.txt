cmake_minimum_required(VERSION 3.20)
project(OpenTrackIR VERSION 0.2.4 LANGUAGES C CXX)

option(OPENTRACKIR_BUILD_TESTS "Build OpenTrackIR native tests" ON)
option(OPENTRACKIR_BUILD_PREVIEW "Build the OpenCV preview harness when dependencies are present" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(LIBUSB QUIET IMPORTED_TARGET libusb-1.0)
endif()

if(LIBUSB_FOUND)
  add_library(opentrackir_libusb INTERFACE)
  target_link_libraries(opentrackir_libusb INTERFACE PkgConfig::LIBUSB)
else()
  find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES libusb-1.0)
  find_library(LIBUSB_LIBRARY NAMES usb-1.0 libusb-1.0)
  if(NOT LIBUSB_INCLUDE_DIR OR NOT LIBUSB_LIBRARY)
    message(FATAL_ERROR "libusb-1.0 is required to build the native TrackIR library")
  endif()

  add_library(opentrackir_libusb INTERFACE)
  target_include_directories(opentrackir_libusb INTERFACE "${LIBUSB_INCLUDE_DIR}")
  target_link_libraries(opentrackir_libusb INTERFACE "${LIBUSB_LIBRARY}")
endif()

if(OPENTRACKIR_BUILD_TESTS)
  enable_testing()
endif()

find_package(OpenCV QUIET COMPONENTS core highgui imgproc)

add_subdirectory(c)
if(OPENTRACKIR_BUILD_PREVIEW)
  add_subdirectory(cpp)
endif()
