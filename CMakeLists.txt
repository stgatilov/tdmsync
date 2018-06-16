cmake_minimum_required (VERSION 3.9.6)
project(tdmsync C CXX)

set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo" CACHE STRING "" FORCE)

set(lib_sources
    tdmsync.h
    tdmsync.cpp
    sha1.c
    sha1.h
    buzhash.c
    buzhash.h
    binsearch.c
    binsearch.h
    phf.h
)

set(test_sources
    main.cpp
)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W2")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Ob2 /FAs")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()


add_library(libtdmsync ${lib_sources})

add_executable(tdmsync ${test_sources})
target_link_libraries(tdmsync libtdmsync)
