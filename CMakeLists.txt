cmake_minimum_required(VERSION 3.9.6)

project(perfoscope VERSION 0.1.0 LANGUAGES C CXX)

if(NOT MODULEFILE_PREFIX)
  message(FATAL_ERROR "Specify installation prefix for modulefiles by setting MODULEFILE_PREFIX")
endif()

find_package(SQLITE 3.21.0)
find_package(PAPI 5.5.1)

# Installation directories
if(UNIX AND NOT APPLE)
  set(INSTALL_BIN_DIR "perfoscope/0.1.0/bin")
  set(INSTALL_LIB_DIR "perfoscope/0.1.0/lib")
  set(INSTALL_INCLUDE_DIR "perfoscope/0.1.0/include")
  set(INSTALL_CONFIG_DIR "perfoscope/0.1.0/lib/cmake/perfoscope")
else()
  message(ERROR "Unsupported operating system")
endif()

# perfoscope library
add_library(perfoscope STATIC perfoscope.cpp texttable.cpp)
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
#target_link_libraries(perfoscope PUBLIC -lrt)
target_compile_features(perfoscope PUBLIC cxx_std_11)

if(SQLITE_FOUND)
  target_include_directories(perfoscope PUBLIC ${SQLITE_INCLUDE_DIRS})
#  target_link_libraries(perfoscope PUBLIC ${SQLITE_LIBRARIES})
  target_compile_definitions(
    perfoscope
    PUBLIC
    USING_PERFOSCOPE_DBSTORE
  )
endif()

if(PAPI_FOUND)
  target_include_directories(perfoscope PUBLIC ${PAPI_INCLUDE_DIRS})
#  target_link_libraries(perfoscope PUBLIC ${PAPI_LIBRARIES})
  target_compile_definitions(
    perfoscope
    PUBLIC
    USING_PERFOSCOPE_HWC
  )
endif()

# Generate configuration files
configure_file("modulefile.lua.in" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_VERSION}.lua" @ONLY)
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

# Install modulefile
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_VERSION}.lua" DESTINATION "${MODULEFILE_PREFIX}/perfoscope")

# Install targets
install(
  TARGETS perfoscope
  EXPORT perfoscope-targets
  ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
  LIBRARY DESTINATION ${INSTALL_LIB_DIR}
)

# Install export
install(
  EXPORT perfoscope-targets
  DESTINATION "${INSTALL_CONFIG_DIR}"
)
