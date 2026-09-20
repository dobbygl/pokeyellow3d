# Integrate through the runtime's versioned API; fetched sources stay untouched.
find_package(SDL2 REQUIRED)
get_target_property(runtime_dir gbrt SOURCE_DIR)
set(presentation_header "${runtime_dir}/include/gb_presentation.h")
if(NOT EXISTS "${presentation_header}")
    message(FATAL_ERROR "Pallet 3D requires the gb-recompiled presentation API v1")
endif()
file(STRINGS "${presentation_header}" presentation_version
    REGEX "^#define GB_PRESENTATION_API_VERSION 1$")
if(NOT presentation_version)
    message(FATAL_ERROR "Pallet 3D: unsupported runtime presentation API version")
endif()
target_sources(gbrt PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pallet3d.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pallet_presentation.cpp")
target_include_directories(gbrt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
