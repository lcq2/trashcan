cmake_minimum_required(VERSION 3.0)
project(pdetect)

set(BOOST_REQUIRED_COMPONENTS
    program_options
    system
    thread
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-mavx2 -DNDEBUG -O3 -rdynamic")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

find_package(Boost "1.46" REQUIRED ${BOOST_REQUIRED_COMPONENTS})
find_package(UHD "3.8.0" REQUIRED)

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${UHD_INCLUDE_DIRS}
)
link_directories(${Boost_LIBRARY_DIRS})

set(SRC_FILES
    main.cpp
    gsmband.cpp
    xarfcnscanner.cpp
)

add_executable(pdetect ${SRC_FILES})

target_link_libraries(pdetect ${UHD_LIBRARIES} ${Boost_LIBRARIES} fftw3f)