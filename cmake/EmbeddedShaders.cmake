set(_live2d_shader_root "${LIVE2D_CUBISM_NATIVE_ROOT}/Framework/src/Rendering/${FRAMEWORK_SOURCE}/Shaders")
set(_live2d_shader_prefix "FrameworkShaders/")
if (FRAMEWORK_SOURCE STREQUAL "D3D11")
    set(_live2d_shader_directory "${_live2d_shader_root}")
    set(_live2d_shader_files
            "${_live2d_shader_directory}/CubismEffect.fx"
            "${_live2d_shader_directory}/CubismBlendMode.fx")
elseif (ANDROID)
    set(_live2d_shader_directory "${_live2d_shader_root}/StandardES")
    set(_live2d_shader_prefix "")
else ()
    set(_live2d_shader_directory "${_live2d_shader_root}/Standard")
endif ()

if (FRAMEWORK_SOURCE STREQUAL "OpenGL")
    file(GLOB _live2d_shader_files CONFIGURE_DEPENDS
            "${_live2d_shader_directory}/*.vert" "${_live2d_shader_directory}/*.frag")
endif ()
if (NOT _live2d_shader_files)
    message(FATAL_ERROR "Cubism ${FRAMEWORK_SOURCE} shaders are missing: ${_live2d_shader_directory}")
endif ()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_live2d_shader_files})
set(_live2d_shader_header "#pragma once\n#include <string_view>\nnamespace huxerui::live2d::detail {\nstruct EmbeddedShader { std::string_view path; std::string_view source; };\ninline constexpr EmbeddedShader embedded_shaders[]{\n")
foreach (_live2d_shader IN LISTS _live2d_shader_files)
    get_filename_component(_live2d_shader_name "${_live2d_shader}" NAME)
    file(READ "${_live2d_shader}" _live2d_shader_source)
    string(APPEND _live2d_shader_header
            "{\"${_live2d_shader_prefix}${_live2d_shader_name}\", R\"cubism_shader(${_live2d_shader_source})cubism_shader\"},\n")
endforeach ()
string(APPEND _live2d_shader_header "};\n}\n")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/live2d-generated/embedded_shaders.h"
        CONTENT "${_live2d_shader_header}" @ONLY)
target_include_directories(huxerui_live2d PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/live2d-generated")
target_compile_definitions(huxerui_live2d PRIVATE LIVE2D_EMBEDDED_SHADERS)
