#
#  CMakeLists.txt
#  applaudio
#
#  Disclaimer: AI-generated script. Not tested at all.
#
#  Created by Rasmus Anthin on 2025-09-15.
#

cmake_minimum_required(VERSION 3.12)
project(applaudio VERSION 1.0.0 LANGUAGES CXX DESCRIPTION "Header-only cross-platform audio library")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create interface library for header-only
add_library(applaudio INTERFACE)
add_library(applaudio::applaudio ALIAS applaudio)

# Add include directories
target_include_directories(applaudio INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Platform-specific system libraries
if(APPLE)
    target_link_libraries(applaudio INTERFACE 
        "-framework AudioToolbox -framework CoreAudio -framework CoreFoundation"
    )
    
elseif(WIN32)
    target_link_libraries(applaudio INTERFACE ole32)
    
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED alsa)
    
    target_include_directories(applaudio INTERFACE ${ALSA_INCLUDE_DIRS})
    target_link_libraries(applaudio INTERFACE ${ALSA_LIBRARIES})
    
endif()

# Install headers
install(DIRECTORY include/ DESTINATION include)

# Export for other projects
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/applaudioConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(TARGETS applaudio EXPORT applaudioTargets)
install(EXPORT applaudioTargets
    FILE applaudioTargets.cmake
    DESTINATION lib/cmake/applaudio
)

# Create config file
configure_file(cmake/applaudioConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/applaudioConfig.cmake
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/applaudioConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/applaudioConfigVersion.cmake
    DESTINATION lib/cmake/applaudio
)
