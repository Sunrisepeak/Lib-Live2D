set(_shader_source "${LIVE2D_CUBISM_NATIVE_ROOT}/Framework/src/Rendering/Metal/Shaders")
set(_shader_output "${CMAKE_CURRENT_BINARY_DIR}/cubism-shaders/FrameworkMetallibs")
set(_color_modes Normal Add AddGlow Darken Multiply ColorBurn LinearBurn Lighten Screen ColorDodge
        Overlay SoftLight HardLight LinearLight Hue Color)
set(_alpha_modes Over Atop Out ConjointOver DisjointOver)
file(GLOB _shader_dependencies "${_shader_source}/*.h" "${_shader_source}/*.metal")
set(_compiled_shaders)

function(_live2d_compile_shader name output)
    set(_binary "${_shader_output}/${output}.metallib")
    set(_intermediate "${_shader_output}/${output}.air")
    add_custom_command(OUTPUT "${_binary}"
            BYPRODUCTS "${_intermediate}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${_shader_output}"
            COMMAND xcrun -sdk "${PLATFORM_NAME}" metal ${ARGN}
                -c "${_shader_source}/${name}.metal" -o "${_intermediate}"
            COMMAND xcrun -sdk "${PLATFORM_NAME}" metallib "${_intermediate}" -o "${_binary}"
            DEPENDS ${_shader_dependencies}
            VERBATIM)
    set(_compiled_shaders ${_compiled_shaders} "${_binary}" PARENT_SCOPE)
endfunction()

foreach(_shader MetalShaders VertShaderSrcBlend VertShaderSrcMaskedBlend)
    _live2d_compile_shader("${_shader}" "${_shader}")
endforeach()
foreach(_shader FragShaderSrcBlend FragShaderSrcMaskBlend FragShaderSrcMaskInvertedBlend
        FragShaderSrcMaskInvertedPremultipliedAlphaBlend FragShaderSrcMaskPremultipliedAlphaBlend
        FragShaderSrcPremultipliedAlphaBlend)
    foreach(_color RANGE 0 15)
        list(GET _color_modes ${_color} _color_name)
        foreach(_alpha RANGE 0 4)
            if (_color EQUAL 0 AND _alpha EQUAL 0)
                continue()
            endif ()
            list(GET _alpha_modes ${_alpha} _alpha_name)
            _live2d_compile_shader("${_shader}" "${_shader}${_color_name}${_alpha_name}"
                    -D "CSM_COLOR_BLEND_MODE=${_color}" -D "CSM_ALPHA_BLEND_MODE=${_alpha}")
        endforeach()
    endforeach()
endforeach()
add_custom_target(live2d_metal_shaders DEPENDS ${_compiled_shaders})
set_property(TARGET huxerui_live2d PROPERTY LIVE2D_METAL_LIBRARIES "${_compiled_shaders}")
add_dependencies(huxerui_live2d live2d_metal_shaders)

function(live2d_configure_app target)
    get_target_property(_shaders huxerui_live2d LIVE2D_METAL_LIBRARIES)
    set_source_files_properties(${_shaders} TARGET_DIRECTORY ${target} PROPERTIES
            GENERATED TRUE MACOSX_PACKAGE_LOCATION "Resources/FrameworkMetallibs")
    target_sources(${target} PRIVATE ${_shaders})
    add_dependencies(${target} live2d_metal_shaders)
    if (IOS)
        list(GET _shaders 0 _first_shader)
        get_filename_component(_compiled_directory "${_first_shader}" DIRECTORY)
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "${_compiled_directory}" "${CMAKE_BINARY_DIR}/live2d-ios/FrameworkMetallibs"
                VERBATIM)
    endif ()
endfunction()
