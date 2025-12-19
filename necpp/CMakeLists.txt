cmake_minimum_required(VERSION 3.10)
project(nec2wasm)

set(CMAKE_CXX_STANDARD 11)

include_directories(src)

set(NEC_SOURCES
    src/c_evlcom.cpp
    src/c_geometry.cpp
    src/c_ggrid.cpp
    src/c_plot_card.cpp
    src/libNEC.cpp
    src/matrix_algebra.cpp
    src/misc.cpp
    src/nec_context.cpp
    src/nec_exception.cpp
    src/nec_ground.cpp
    src/nec_output.cpp
    src/nec_radiation_pattern.cpp
    src/nec_structure_currents.cpp
    src/electromag.cpp
    src/nec_results.cpp
)

add_compile_options("-fexceptions")

add_executable(nec_engine
    bindings.cpp 
    ${NEC_SOURCES}
)

target_link_options(nec_engine PRIVATE
    "-lembind"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME='createNecEngine'"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sWASM=1"
    "-fexceptions"
    "-sDISABLE_EXCEPTION_CATCHING=0"
    "-O3"
)