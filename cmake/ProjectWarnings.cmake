# Only project-owned presentation and tests are warning-clean build gates.
# The upstream runtime and generated cartridge sources keep their own policies.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(project_warnings -Wall -Wextra -Werror)
elseif(MSVC)
    set(project_warnings /W4 /WX)
    # The runtime's public register layout uses deliberate anonymous structs.
    # Treat upstream headers as external while retaining /W4 /WX on owned code.
    get_target_property(runtime_source gbrt SOURCE_DIR)
    target_include_directories(gbrt SYSTEM PUBLIC
        "${runtime_source}/include" "${runtime_source}/vendor" "${runtime_source}/vendor/imgui")
else()
    return()
endif()

if(POKEYELLOW_3D)
    # These sources belong to the runtime target, whose directory owns their
    # source properties. CMake 3.18 provides TARGET_DIRECTORY for this case.
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/pallet3d.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/pallet_presentation.cpp"
        TARGET_DIRECTORY gbrt APPEND PROPERTY COMPILE_OPTIONS ${project_warnings})
    if(MSVC)
        set_property(SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/pallet3d.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/pallet_presentation.cpp"
            TARGET_DIRECTORY gbrt APPEND PROPERTY COMPILE_DEFINITIONS _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

get_property(project_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(project_target IN LISTS project_targets)
    get_target_property(project_sources ${project_target} SOURCES)
    foreach(project_source IN LISTS project_sources)
        if(project_source MATCHES "(^|/)tests/.*\\.(cpp|c)$")
            target_compile_options(${project_target} PRIVATE ${project_warnings})
            if(MSVC)
                target_include_directories(${project_target} SYSTEM PRIVATE "${runtime_source}/include")
                target_compile_definitions(${project_target} PRIVATE _CRT_SECURE_NO_WARNINGS)
            endif()
            break()
        endif()
    endforeach()
endforeach()
