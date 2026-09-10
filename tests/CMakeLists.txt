cmake_minimum_required(VERSION 3.20)

project(live2d_contract_tests LANGUAGES CXX)

find_package(HuxerUI CONFIG REQUIRED COMPONENTS static testing)

if (NOT APPLE OR IOS)
    message(FATAL_ERROR "The initial texture host contract probe requires macOS")
endif ()

enable_testing()

add_executable(live2d_host_contract host_contract.cpp)
target_compile_features(live2d_host_contract PRIVATE cxx_std_20)
target_link_libraries(live2d_host_contract PRIVATE HuxerUI::testing)
huxerui_enable_codegen(live2d_host_contract)

add_test(NAME live2d_host_contract COMMAND live2d_host_contract)
