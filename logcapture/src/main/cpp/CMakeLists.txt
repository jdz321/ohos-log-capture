# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(log-capture)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

add_library(logcapture SHARED log_capture.cpp)

# libace_napi.z.so: NAPI；libhilog_ndk.z.so: OH_LOG_SetCallback / OH_LOG_Print
target_link_libraries(logcapture PUBLIC libace_napi.z.so libhilog_ndk.z.so)