cmake_minimum_required(VERSION 3.13)
project(tetra_decoder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

file(GLOB SRC "src/*.cpp")

if (NOT DEFINED SDRPP_MODULE_CMAKE)
    if (NOT DEFINED SDRPP_ROOT)
        if (EXISTS "${CMAKE_SOURCE_DIR}/.sdrpp-upstream/CMakeLists.txt")
            set(SDRPP_ROOT "${CMAKE_SOURCE_DIR}/.sdrpp-upstream")
        endif ()
    endif ()

    if (NOT SDRPP_ROOT)
        message(FATAL_ERROR "SDRPP_ROOT is not set. Point it to an SDR++ source tree.")
    endif ()

    if (NOT EXISTS "${SDRPP_ROOT}/sdrpp_module.cmake")
        message(FATAL_ERROR "Could not find sdrpp_module.cmake in SDRPP_ROOT (${SDRPP_ROOT}).")
    endif ()

    set(SDRPP_MODULE_CMAKE "${SDRPP_ROOT}/sdrpp_module.cmake")
endif ()

if (NOT DEFINED SDRPP_CORE_ROOT)
    if (SDRPP_ROOT)
        set(SDRPP_CORE_ROOT "${SDRPP_ROOT}/core")
    else ()
        message(FATAL_ERROR "SDRPP_CORE_ROOT is not set. Point it to an SDR++ core source tree.")
    endif ()
endif ()

if (NOT EXISTS "${SDRPP_CORE_ROOT}")
    message(FATAL_ERROR "SDRPP_CORE_ROOT does not exist: ${SDRPP_CORE_ROOT}")
endif ()

if ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
    if (MSVC)
        set(SDRPP_COMPILER_FLAGS /std:c++17 /EHsc)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SDRPP_COMPILER_FLAGS -g -Og -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
    else ()
        set(SDRPP_COMPILER_FLAGS -g -Og -std=c++17)
    endif ()
else ()
    if (MSVC)
        set(SDRPP_COMPILER_FLAGS /O2 /Ob2 /std:c++17 /EHsc)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SDRPP_COMPILER_FLAGS -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
    else ()
        set(SDRPP_COMPILER_FLAGS -O3 -std=c++17)
    endif ()
endif ()

set(SDRPP_MODULE_COMPILER_FLAGS ${SDRPP_COMPILER_FLAGS})

if (NOT TARGET sdrpp_core AND NOT DEFINED SDRPP_LIB_ROOT)
    if (NOT DEFINED OPT_BACKEND_GLFW)
        set(OPT_BACKEND_GLFW ON)
    endif ()
    if (NOT DEFINED OPT_BACKEND_ANDROID)
        set(OPT_BACKEND_ANDROID OFF)
    endif ()
    if (NOT DEFINED OPT_OVERRIDE_STD_FILESYSTEM)
        set(OPT_OVERRIDE_STD_FILESYSTEM OFF)
    endif ()
    if (NOT DEFINED USE_INTERNAL_LIBCORRECT)
        set(USE_INTERNAL_LIBCORRECT ON)
    endif ()
    if (NOT DEFINED USE_BUNDLE_DEFAULTS)
        set(USE_BUNDLE_DEFAULTS OFF)
    endif ()

    add_subdirectory("${SDRPP_CORE_ROOT}" "${CMAKE_BINARY_DIR}/sdrpp_core")
endif ()

include(${SDRPP_MODULE_CMAKE})

target_include_directories(${PROJECT_NAME} PRIVATE "src")

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

if (UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif ()
