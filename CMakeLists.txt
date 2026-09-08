cmake_minimum_required(VERSION 3.20)

project(l4n_hlae_plugin LANGUAGES CXX)

option(L4N_BUILD_HLAE_PLUGIN
    "Build the source-modified HLAE plugin instead of the adapter baseline"
    OFF)

option(L4N_BUNDLE_OPENEXR
    "Statically link OpenEXR into the native L4N plugin"
    ON)

option(L4N_PACKAGE_RELEASE
    "Create a copy-ready Release package after building the native plugin"
    ON)

set(L4N_VS_INSTALL_PATH "" CACHE PATH
    "Visual Studio installation path; empty enables automatic detection")

set(L4N_SHADERBUILDER_FRAMEWORK_VERSION "v4.6.2" CACHE STRING
    "Target .NET Framework version used by ShaderBuilder")

set(L4N_RELEASE_DIR "${CMAKE_SOURCE_DIR}/Release" CACHE PATH
    "Output directory for the copy-ready L4N HLAE package")

set(L4N_HLAE_CORE_SOURCE "${CMAKE_SOURCE_DIR}/advancedfx/resources" CACHE PATH
    "Tracked HLAE resource directory to include in the package")

if(NOT WIN32)
    message(FATAL_ERROR "l4n_hlae_plugin requires Windows")
endif()

if(NOT MSVC)
    message(FATAL_ERROR
        "l4n_hlae_plugin must be built with MSVC to match the L4N x86 C++ ABI")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "l4n_hlae_plugin must be configured for a 32-bit target")
endif()

if(L4N_BUILD_HLAE_PLUGIN)
    if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
        message(FATAL_ERROR "The HLAE plugin must be configured with the Win32 platform")
    endif()

    # Keep the complete plugin and its static dependencies on the same CRT.
    # This avoids requiring the L4N host to provide the MSVC C++ runtime ABI.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING
        "Use the static MSVC runtime for the native HLAE plugin" FORCE)

    # HLAE invokes MSBuild for ShaderBuilder. One project-level path keeps
    # the helper build independent from the machine-specific VS installation.
    if(L4N_VS_INSTALL_PATH)
        file(TO_CMAKE_PATH "${L4N_VS_INSTALL_PATH}" L4N_VS_INSTALL_PATH_NORMALIZED)
        if(NOT IS_DIRECTORY "${L4N_VS_INSTALL_PATH_NORMALIZED}")
            message(FATAL_ERROR
                "L4N_VS_INSTALL_PATH does not exist: ${L4N_VS_INSTALL_PATH}")
        endif()

        set(AFX_VS_INSTALLPATH "${L4N_VS_INSTALL_PATH_NORMALIZED}" CACHE PATH
            "Visual Studio installation path used by HLAE helper projects" FORCE)

        set(_l4n_msbuild_candidates
            "${L4N_VS_INSTALL_PATH_NORMALIZED}/MSBuild/Current/Bin/MSBuild.exe"
            "${L4N_VS_INSTALL_PATH_NORMALIZED}/MSBuild/Current/Bin/amd64/MSBuild.exe")
        unset(_l4n_msbuild_path)
        foreach(_l4n_msbuild_candidate IN LISTS _l4n_msbuild_candidates)
            if(EXISTS "${_l4n_msbuild_candidate}")
                set(_l4n_msbuild_path "${_l4n_msbuild_candidate}")
                break()
            endif()
        endforeach()

        if(NOT _l4n_msbuild_path)
            message(FATAL_ERROR
                "Could not find MSBuild.exe below L4N_VS_INSTALL_PATH: "
                "${L4N_VS_INSTALL_PATH}")
        endif()

        set(AFX_VS_MSBUILD "${_l4n_msbuild_path}" CACHE FILEPATH
            "MSBuild executable used by HLAE helper projects" FORCE)
    endif()

    set(AFX_SHADERBUILDER_FRAMEWORK_VERSION
        "${L4N_SHADERBUILDER_FRAMEWORK_VERSION}" CACHE STRING
        "Target framework version used to build ShaderBuilder" FORCE)

    set(AFX_L4N_PLUGIN_ONLY ON CACHE BOOL
        "Build only the x86 AfxHookSource target as an L4N plugin" FORCE)
    set(AFX_L4N_STATIC_OPENEXR "${L4N_BUNDLE_OPENEXR}" CACHE BOOL
        "Link OpenEXR statically into the native L4N plugin" FORCE)
    add_subdirectory(advancedfx advancedfx-build)

    if(L4N_PACKAGE_RELEASE)
        add_custom_target(l4n_release_package ALL
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_PATH=$<TARGET_FILE:AfxHookSource>"
                "-DRELEASE_DIR=${L4N_RELEASE_DIR}"
                "-DCORE_SOURCE=${L4N_HLAE_CORE_SOURCE}"
                "-DSHADER_SOURCE=${CMAKE_SOURCE_DIR}/advancedfx/shaders/build"
                "-DCONFIG_SOURCE=${CMAKE_SOURCE_DIR}/config/l4n_hlae_plugin.ini.example"
                "-DLICENSE_SOURCE=${CMAKE_SOURCE_DIR}/advancedfx/LICENSE"
                "-DCREDITS_SOURCE=${CMAKE_SOURCE_DIR}/advancedfx/CREDITS.md"
                -P "${CMAKE_SOURCE_DIR}/cmake/package_release.cmake"
            DEPENDS AfxHookSource
            COMMENT "Packaging copy-ready L4N HLAE Release"
            VERBATIM
        )
    endif()

    return()
endif()

add_library(l4n_hlae_plugin SHARED
    src/l4n_hlae_plugin.cpp
)

target_include_directories(l4n_hlae_plugin PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(l4n_hlae_plugin PRIVATE cxx_std_17)

target_compile_definitions(l4n_hlae_plugin PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0601
)

target_compile_options(l4n_hlae_plugin PRIVATE
    /W4
    /WX
    /EHsc
    /GR-
    /permissive-
)

target_link_options(l4n_hlae_plugin PRIVATE
    /SUBSYSTEM:WINDOWS
)

set_property(TARGET l4n_hlae_plugin PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded"
)

set_target_properties(l4n_hlae_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "l4n_hlae_plugin"
)
