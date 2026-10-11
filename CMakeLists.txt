cmake_minimum_required(VERSION 3.10)
project(InstagramCpp)

set(CMAKE_CXX_STANDARD 17)

# Force static linking
set(BUILD_SHARED_LIBS OFF)
set(CURL_USE_STATIC_LIBS ON)

# Add executable
add_executable(instagram
    instagram.cpp
    main.cpp
)

# For vcpkg (Windows) - use automatic integration
if(DEFINED VCPKG_TOOLCHAIN)
    # vcpkg will automatically provide include paths and libraries
    # Just link to the libraries by name
    target_link_libraries(instagram PRIVATE
        curl
        jsoncpp
    )

    # Manually add vcpkg include directories (just in case)
    target_include_directories(instagram PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows-static/include
    )
else()
    # For Linux - use system packages
    find_package(CURL REQUIRED)

    find_package(JsonCpp 1.9.4 CONFIG)
    if(NOT JsonCpp_FOUND)
        find_path(JsonCpp_INCLUDE_DIR NAMES json/json.h
            PATHS /usr/include/jsoncpp /usr/local/include/jsoncpp
        )
        find_library(JsonCpp_LIBRARY NAMES jsoncpp
            PATHS /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib
        )
        if(NOT JsonCpp_INCLUDE_DIR OR NOT JsonCpp_LIBRARY)
            message(FATAL_ERROR "JsonCpp not found. Install with: sudo apt-get install libjsoncpp-dev")
        endif()
        target_link_libraries(instagram PRIVATE
            ${CURL_LIBRARIES}
            ${JsonCpp_LIBRARY}
        )
        target_include_directories(instagram PRIVATE
            ${CURL_INCLUDE_DIRS}
            ${JsonCpp_INCLUDE_DIR}
        )
    else()
        target_link_libraries(instagram PRIVATE
            CURL::libcurl
            JsonCpp::JsonCpp
        )
    endif()
endif()
