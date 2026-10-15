cmake_minimum_required(VERSION 3.24)
project(pcap_alignment LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pcap src/main.cpp src/scalar.cpp src/traceback.cpp src/validation.cpp)
target_include_directories(pcap PRIVATE include)
target_compile_options(pcap PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>"
)

enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(
    NAME scalar_correctness
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/test_scalar.py
)
set_tests_properties(
    scalar_correctness
    PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME affine_correctness
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/test_affine.py
)
set_tests_properties(
    affine_correctness
    PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

find_package(OpenMP REQUIRED COMPONENTS CXX)
target_link_libraries(pcap PRIVATE OpenMP::OpenMP_CXX)

add_test(
    NAME openmp_correctness
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/test_affine.py openmp
)
set_tests_properties(
    openmp_correctness
    PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        ENVIRONMENT "OMP_NUM_THREADS=4"
)

foreach(tile IN ITEMS 1 5 16 32)
    add_test(
        NAME openmp_tiled_${tile}
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/test_affine.py openmp-tiled ${tile}
    )
    set_tests_properties(
        openmp_tiled_${tile}
        PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            ENVIRONMENT "OMP_NUM_THREADS=4"
    )
endforeach()

enable_language(CUDA)
set_target_properties(pcap PROPERTIES
    CUDA_STANDARD 17
    CUDA_STANDARD_REQUIRED ON
    CUDA_ARCHITECTURES native
)

target_sources(pcap PRIVATE src/cuda.cu)

add_test(
    NAME cuda_correctness
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/test_affine.py cuda
)
set_tests_properties(
    cuda_correctness
    PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

foreach(tile IN ITEMS 5 32)
    add_test(
        NAME cuda_tiled_${tile}
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/test_affine.py cuda-tiled ${tile}
    )
    set_tests_properties(
        cuda_tiled_${tile}
        PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()
