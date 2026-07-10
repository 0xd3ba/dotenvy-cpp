cmake_minimum_required(VERSION 3.21)
project(
    Dotenvy
    VERSION 1.0.0
    DESCRIPTION "A small, header-only, dependency-free .env file parser for modern C++"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(DOTENVY_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(DOTENVY_BUILD_EXAMPLES "Build example programs" ${PROJECT_IS_TOP_LEVEL})

add_library(dotenvy INTERFACE)
add_library(Dotenvy::dotenvy ALIAS dotenvy)

target_include_directories(dotenvy INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(dotenvy INTERFACE cxx_std_17)

if(DOTENVY_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

if(DOTENVY_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(
    TARGETS dotenvy
    EXPORT DotenvyTargets
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
    EXPORT DotenvyTargets
    FILE DotenvyTargets.cmake
    NAMESPACE Dotenvy::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Dotenvy
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DotenvyConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/DotenvyConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Dotenvy
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/DotenvyConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/DotenvyConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/DotenvyConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Dotenvy
)
