cmake_minimum_required(VERSION 3.0)
project(risc-666)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/${CMAKE_BUILD_TYPE}/)

include_directories(${CMAKE_SOURCE_DIR}/src)
add_subdirectory(src)