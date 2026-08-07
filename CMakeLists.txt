cmake_minimum_required(VERSION 3.24)

project(applaudio VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

add_library(applaudio INTERFACE)
add_library(applaudio::applaudio ALIAS applaudio)

target_compile_features(applaudio INTERFACE cxx_std_20)
target_include_directories(applaudio INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(APPLE)
  find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
  find_library(COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  target_link_libraries(applaudio INTERFACE
    "${AUDIOTOOLBOX_FRAMEWORK}"
    "${COREAUDIO_FRAMEWORK}"
    "${COREFOUNDATION_FRAMEWORK}"
  )
elseif(WIN32)
  target_link_libraries(applaudio INTERFACE ole32)
elseif(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
  target_link_libraries(applaudio INTERFACE PkgConfig::ALSA)
endif()

install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(TARGETS applaudio EXPORT applaudioTargets)
install(EXPORT applaudioTargets
  FILE applaudioTargets.cmake
  NAMESPACE applaudio::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/applaudio"
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  cmake/applaudioConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/applaudioConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/applaudio"
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/applaudioConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/applaudioConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/applaudioConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/applaudio"
)
