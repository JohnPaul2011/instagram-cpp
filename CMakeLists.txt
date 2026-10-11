cmake_minimum_required(VERSION 3.10)
project(InstagramCpp)

set(CMAKE_CXX_STANDARD 17)

# Disable MSVC warnings
if(MSVC)
    add_compile_options(/wd4244 /wd4996)
endif()

add_executable(instagram instagram.cpp main.cpp)

# For vcpkg (Windows)
if(DEFINED VCPKG_TOOLCHAIN)
    # Add vcpkg include directory
    target_include_directories(instagram PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows-static/include
    )
    target_link_libraries(instagram PRIVATE zlib curl jsoncpp)
else()
    find_package(CURL REQUIRED)
    find_package(JsonCpp REQUIRED)
    target_link_libraries(instagram PRIVATE CURL::libcurl JsonCpp::JsonCpp)
endif()
