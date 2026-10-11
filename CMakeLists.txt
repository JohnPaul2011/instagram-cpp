cmake_minimum_required(VERSION 3.10)
project(InstagramCpp)

set(CMAKE_CXX_STANDARD 17)

# Find CURL
find_package(CURL REQUIRED)

# Find JsonCpp with multiple strategies
find_package(JsonCpp 1.9.4 REQUIRED)
if(NOT JsonCpp_FOUND)
    # Try pkg-config
    find_package(PkgConfig)
    pkg_check_modules(JsonCpp jsoncpp)
    if(JsonCpp_FOUND)
        add_definitions(${JsonCpp_DEFINITIONS})
        include_directories(${JsonCpp_INCLUDE_DIRS})
        link_directories(${JsonCpp_LIBRARY_DIRS})
        set(JsonCpp_LIBRARIES ${JsonCpp_LIBRARIES})
    else()
        # Fallback: Use system paths
        find_path(JsonCpp_INCLUDE_DIR NAMES json/json.h)
        find_library(JsonCpp_LIBRARY NAMES jsoncpp)
        if(JsonCpp_INCLUDE_DIR AND JsonCpp_LIBRARY)
            set(JsonCpp_FOUND TRUE)
            set(JsonCpp_INCLUDE_DIRS ${JsonCpp_INCLUDE_DIR})
            set(JsonCpp_LIBRARIES ${JsonCpp_LIBRARY})
        else()
            message(FATAL_ERROR "JsonCpp not found. Install it with: sudo apt-get install libjsoncpp-dev")
        endif()
    endif()
endif()

# Add executable
add_executable(instagram
    instagram.cpp
)

# Link libraries
target_link_libraries(instagram
    ${CURL_LIBRARIES}
    ${JsonCpp_LIBRARIES}
)

# Include directories
target_include_directories(instagram PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${JsonCpp_INCLUDE_DIRS}
)
