cmake_minimum_required(VERSION 3.20)

foreach(required_variable IN ITEMS
        PLUGIN_PATH
        RELEASE_DIR
        CORE_SOURCE
        SHADER_SOURCE
        CONFIG_SOURCE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "Missing package variable: ${required_variable}")
    endif()
endforeach()

foreach(required_path IN ITEMS PLUGIN_PATH CORE_SOURCE CONFIG_SOURCE)
    if(NOT EXISTS "${${required_path}}")
        message(FATAL_ERROR
            "Release package input does not exist: ${${required_path}}")
    endif()
endforeach()

set(plugin_destination "${RELEASE_DIR}/bin/neko/plugins")
set(core_destination "${RELEASE_DIR}/l4n_hlae_core")

# These directories are generated output. Remove only the package-owned trees
# so a rebuild cannot leave stale DLLs or resources behind.
file(REMOVE_RECURSE "${plugin_destination}" "${core_destination}")
file(MAKE_DIRECTORY "${plugin_destination}" "${core_destination}")

file(COPY_FILE
    "${PLUGIN_PATH}"
    "${plugin_destination}/l4n_hlae_plugin.dll"
)
file(COPY_FILE
    "${CONFIG_SOURCE}"
    "${plugin_destination}/l4n_hlae_plugin.ini"
)

file(GLOB_RECURSE core_files
    LIST_DIRECTORIES false
    RELATIVE "${CORE_SOURCE}"
    "${CORE_SOURCE}/*"
)
foreach(relative_file IN LISTS core_files)
    get_filename_component(relative_directory "${relative_file}" DIRECTORY)
    file(MAKE_DIRECTORY "${core_destination}/${relative_directory}")
    file(COPY_FILE
        "${CORE_SOURCE}/${relative_file}"
        "${core_destination}/${relative_file}"
    )
endforeach()

# ShaderBuilder generates the binary shader combinations in the source tree.
# They are runtime resources, not compiler headers, so package only .acs files.
if(EXISTS "${SHADER_SOURCE}")
    file(GLOB generated_shaders "${SHADER_SOURCE}/*.acs")
    file(MAKE_DIRECTORY "${core_destination}/resources/shaders")
    foreach(shader IN LISTS generated_shaders)
        get_filename_component(shader_name "${shader}" NAME)
        file(COPY_FILE
            "${shader}"
            "${core_destination}/resources/shaders/${shader_name}"
        )
    endforeach()
endif()

foreach(attribution_source IN ITEMS LICENSE_SOURCE CREDITS_SOURCE)
    if(DEFINED ${attribution_source} AND EXISTS "${${attribution_source}}")
        get_filename_component(attribution_name "${${attribution_source}}" NAME)
        file(COPY_FILE
            "${${attribution_source}}"
            "${core_destination}/${attribution_name}"
        )
    endif()
endforeach()

if(NOT EXISTS "${core_destination}/resources/hexfont.tga")
    message(FATAL_ERROR
        "Packaged HLAE core is missing resources/hexfont.tga")
endif()

message(STATUS "L4N HLAE Release package: ${RELEASE_DIR}")
