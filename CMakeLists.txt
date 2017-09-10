cmake_minimum_required(VERSION 3.3.1)

project(perfoscope VERSION 0.1.0 LANGUAGES C CXX)

set(sqlite3_DIR "/work/adz8/development/host/sqlite3-3.19.3/lib/cmake/sqlite3")
set(papi_DIR "/work/adz8/development/host/papi-5.5.1/lib/cmake/papi")

find_package(sqlite3 3.19.3)
find_package(papi 5.5.1)

# Installation directories
if(UNIX AND NOT APPLE)
  set(INSTALL_BIN_DIR "perfoscope-0.1.0/bin")
  set(INSTALL_LIB_DIR "perfoscope-0.1.0/lib")
  set(INSTALL_INCLUDE_DIR "perfoscope-0.1.0/include")
  set(INSTALL_CONFIG_DIR "perfoscope-0.1.0/lib/cmake/perfoscope")
else()
  message(ERROR "Unsupported operating system")
endif()

# perfoscope library
add_library(perfoscope SHARED perfoscope.cpp texttable.cpp)
target_include_directories(
  perfoscope
  PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}/perfoscope>
)
target_compile_definitions(
  perfoscope
  PUBLIC
  USING_PERFOSCOPE
  USING_PERFOSCOPE_WCT
)
target_link_libraries(perfoscope PUBLIC -lrt)
target_compile_options(perfoscope PUBLIC -std=c++11)

if(sqlite3_FOUND)
target_link_libraries(perfoscope PUBLIC sqlite3)
target_compile_definitions(
  perfoscope
  PUBLIC
  USING_PERFOSCOPE_DBSTORE
)
endif()

if(papi_FOUND)
target_link_libraries(perfoscope PUBLIC papi)
target_compile_definitions(
  perfoscope
  PUBLIC
  USING_PERFOSCOPE_HWC
)
endif()

# Generate configuration files
include(CMakePackageConfigHelpers)
configure_package_config_file(
  "perfoscope-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/perfoscope-config.cmake"
  INSTALL_DESTINATION "${INSTALL_CONFIG_DIR}"
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/perfoscope-config-version.cmake"
  COMPATIBILITY SameMajorVersion
)

# Install configuration files
# Install configuration files
install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/perfoscope-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/perfoscope-config-version.cmake"
  DESTINATION "${INSTALL_CONFIG_DIR}"
)

# Install header files
install(
  FILES perfoscope.hpp common.hpp texttable.hpp texttablefwd.hpp
  DESTINATION "${INSTALL_INCLUDE_DIR}/perfoscope"
)

# Install targets
install(
  TARGETS perfoscope
  EXPORT perfoscope-targets
  LIBRARY DESTINATION ${INSTALL_LIB_DIR}
)

# Install export
install(
  EXPORT perfoscope-targets
  DESTINATION "${INSTALL_CONFIG_DIR}"
)
