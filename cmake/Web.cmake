find_program(_live2d_node node REQUIRED)
set(_live2d_project "${CMAKE_CURRENT_LIST_DIR}/..")
if (NOT EXISTS "${_live2d_project}/.cache/web-tools/node_modules/esbuild/lib/main.js")
    message(FATAL_ERROR "Run npm install --prefix .cache/web-tools --no-audit --no-fund esbuild@0.25.12 typescript@5.9.3 from the Live2D library")
endif ()
file(GLOB_RECURSE _web_framework_sources "${LIVE2D_CUBISM_WEB_ROOT}/Framework/src/*.ts")
set(_web_bundle "${CMAKE_CURRENT_BINARY_DIR}/live2d-web.js")
set(_web_core "${LIVE2D_CUBISM_WEB_ROOT}/Core/live2dcubismcore.js")
add_custom_command(OUTPUT "${_web_bundle}"
        COMMAND "${_live2d_node}" "${_live2d_project}/tools/build_web.mjs" "${LIVE2D_CUBISM_WEB_ROOT}" "${_web_bundle}"
        DEPENDS "${_live2d_project}/platform/web/bridge.ts" "${_live2d_project}/tools/build_web.mjs" ${_web_framework_sources}
        VERBATIM)
add_custom_target(live2d_web_bridge DEPENDS "${_web_bundle}")
add_dependencies(huxerui_live2d live2d_web_bridge)
target_link_options(huxerui_live2d INTERFACE "SHELL:--extern-pre-js '${_web_core}'" "SHELL:--extern-pre-js '${_web_bundle}'")
set_property(TARGET huxerui_live2d APPEND PROPERTY INTERFACE_LINK_DEPENDS "${_web_core}" "${_web_bundle}")
set_property(TARGET huxerui_live2d PROPERTY LIVE2D_WEB_SHADER_SOURCE "${LIVE2D_CUBISM_WEB_ROOT}/Framework/Shaders/WebGL")

function(live2d_configure_app target)
    get_target_property(_shaders huxerui_live2d LIVE2D_WEB_SHADER_SOURCE)
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_shaders}" "$<TARGET_FILE_DIR:${target}>/live2d-shaders"
            VERBATIM)
endfunction()
