cmake_minimum_required(VERSION 3.10)
project(AOS_vs_SOA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-mtune=native -march=native -Ofast")


find_package(benchmark REQUIRED)

add_executable(benchmark_aos_soa main.cpp)

target_link_libraries(benchmark_aos_soa benchmark::benchmark)
