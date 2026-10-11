cmake_minimum_required(VERSION 3.10)
project(InstagramCpp)

set(CMAKE_CXX_STANDARD 17)

# Force static linking
set(BUILD_SHARED_LIBS OFF)
set(CURL_USE_STATIC_LIBS ON)

# Find CURL (works with both vcpkg and system packages)
find_package(CURL REQUIRED)
if(NOT CURL_FOUND)
    # Fallback for vcpkg
    find_package(CURL CONFIG REQUIRED)
endif()

# Find JsonCpp (works with both vcpkg and system packages)
find_package(JsonCpp CONFIG)
if(NOT JsonCpp_FOUND)
    find_package(JsonCpp 1.9.4 REQUIRED)
    if(NOT JsonCpp_FOUND)
        find_path(JsonCpp_INCLUDE_DIR NAMES json/json.h PATHS /usr/include/jsoncpp)
        find_library(JsonCpp_LIBRARY NAMES jsoncpp PATHS /usr/lib/x86_64-linux-gnu)
        if(JsonCpp_INCLUDE_DIR AND JsonCpp_LIBRARY)
            set(JsonCpp_FOUND TRUE)
            add_library(JsonCpp::JsonCpp STATIC IMPORTED)
            set_target_properties(JsonCpp::JsonCpp PROPERTIES
                IMPORTED_LOCATION ${JsonCpp_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES ${JsonCpp_INCLUDE_DIR}
            )
        else()
            message(FATAL_ERROR "JsonCpp not found. Install with: sudo apt-get install libjsoncpp-dev")
        endif()
    endif()
endif()

# Add executable
add_executable(instagram
    instagram.cpp
    main.cpp
)

# Link libraries
target_link_libraries(instagram
    PRIVATE
    CURL::libcurl
    JsonCpp::JsonCpp
)

# Include directories
target_include_directories(instagram PRIVATE
    ${CURL_INCLUDE_DIRS}
)
