# Build this repository as an out-of-tree SDR++ module against prebuilt SDR++ nightly binaries.
if (NOT SDRPP_CORE_ROOT)
    set(SDRPP_CORE_ROOT "../sdrpp_src/core")
endif ()

if (NOT SDRPP_LIB_ROOT)
    if (MSVC)
        set(SDRPP_LIB_ROOT "../sdrpp_lib/sdrpp_windows_x64")
    else ()
        set(SDRPP_LIB_ROOT "../sdrpp_lib/usr/lib")
    endif ()
endif ()

if (NOT SDRPP_MODULE_COMPILER_FLAGS)
    if ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
        if (MSVC)
            set(SDRPP_MODULE_COMPILER_FLAGS /std:c++17 /EHsc)
        elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(SDRPP_MODULE_COMPILER_FLAGS -g -Og -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
        else ()
            set(SDRPP_MODULE_COMPILER_FLAGS -g -Og -std=c++17)
        endif ()
    else ()
        if (MSVC)
            set(SDRPP_MODULE_COMPILER_FLAGS /O2 /Ob2 /std:c++17 /EHsc)
        elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(SDRPP_MODULE_COMPILER_FLAGS -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
        else ()
            set(SDRPP_MODULE_COMPILER_FLAGS -O3 -std=c++17)
        endif ()
    endif ()
endif ()

add_library(${PROJECT_NAME} SHARED ${SRC})

if (MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE "${SDRPP_LIB_ROOT}/sdrpp_core.lib")
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE "${SDRPP_LIB_ROOT}/libsdrpp_core.so")
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE "${SDRPP_CORE_ROOT}/src/" "${SDRPP_CORE_ROOT}/src/imgui/")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_compile_options(${PROJECT_NAME} PRIVATE ${SDRPP_MODULE_COMPILER_FLAGS})
install(TARGETS ${PROJECT_NAME} DESTINATION lib/sdrpp/plugins)

if (MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

    target_link_directories(${PROJECT_NAME} PUBLIC "C:/Program Files/PothosSDR/lib/")
    target_include_directories(${PROJECT_NAME} PUBLIC "C:/Program Files/PothosSDR/include/")

    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenGL::GL)

    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC glfw)

    find_package(FFTW3f CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC FFTW3::fftw3f)

    find_package(zstd CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC zstd::libzstd_shared)

    target_link_libraries(${PROJECT_NAME} PUBLIC volk wsock32 ws2_32 iphlpapi)

    target_compile_definitions(${PROJECT_NAME} PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _WINSOCKAPI_)
endif ()
