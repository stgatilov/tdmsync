cmake_minimum_required (VERSION 3.9.6)
project(tdmsync C CXX)

option(WITH_CURL "Link with libcurl to support update from URL" ON)

if(WITH_CURL)
    include(conanbuildinfo.cmake)
    conan_basic_setup()
    set(LIBCURL_LIBS ${CONAN_LIBS} "crypt32.lib")
    add_definitions(-DWITH_CURL)
else()
    set(LIBCURL_LIBS )
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo" CACHE STRING "" FORCE)

set(lib_sources
    tdmsync.h
    tdmsync.cpp
    fileio.h
    fileio.cpp
    tsassert.h
    tsassert.cpp
    sha1.c
    sha1.h
    buzhash.c
    buzhash.h
    polyhash.c
    polyhash.h
    binsearch.c
    binsearch.h
    phf.h
)

set(lib_curl_sources
    tdmsync_curl.h
    tdmsync_curl.cpp
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

if(WITH_CURL)
    set(lib_sources ${lib_sources} ${lib_curl_sources})
endif()

add_library(libtdmsync ${lib_sources})
add_executable(tdmsync ${test_sources})
target_link_libraries(tdmsync libtdmsync ${LIBCURL_LIBS})
