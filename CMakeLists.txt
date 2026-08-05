cmake_minimum_required(VERSION 3.14)

project(benchmark_arrow
        VERSION 0.1.0
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (DEFINED VCPKG_ROOT)
    include(${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
endif ()

find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)

add_executable(arrow_write_benchmark
        src/main.cpp
        src/args.cpp
        src/generate.cpp
        src/write.cpp)

target_include_directories(arrow_write_benchmark PRIVATE src)

target_link_libraries(arrow_write_benchmark
        PRIVATE
        "$<IF:$<BOOL:${ARROW_BUILD_STATIC}>,Arrow::arrow_static,Arrow::arrow_shared>"
        "$<IF:$<BOOL:${ARROW_BUILD_STATIC}>,Parquet::parquet_static,Parquet::parquet_shared>")

if (NOT MSVC)
    target_compile_options(arrow_write_benchmark PRIVATE -Wall -Wextra)
endif ()
