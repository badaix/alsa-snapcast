cmake_minimum_required(VERSION 3.10.2)
project(alsa-snapcast LANGUAGES CXX VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

# Includes
include(CheckSymbolExists)
find_package(PkgConfig REQUIRED)

# Libraries

## Boost
find_package(Boost 1.74 REQUIRED)
add_compile_definitions(BOOST_ERROR_CODE_HEADER_ONLY BOOST_ASIO_NO_TS_EXECUTORS)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

## ALSA
pkg_check_modules(alsa REQUIRED IMPORTED_TARGET alsa)
link_directories(${alsa_LIBRARY_DIRS})

# Targets

## ALSA Plugin
add_library(asound_module_pcm_snapcast SHARED pcm_snapcast.cpp snapstream.cpp string_utils.cpp uri.cpp)
target_link_libraries(asound_module_pcm_snapcast PkgConfig::alsa)
### ALSA requires PIC for dynamically linked plugins, so we need to define it.
target_compile_definitions(asound_module_pcm_snapcast PRIVATE -DPIC=1)
set_property(TARGET asound_module_pcm_snapcast PROPERTY POSITION_INDEPENDENT_CODE ON)

install(TARGETS asound_module_pcm_snapcast DESTINATION lib/alsa-lib)
