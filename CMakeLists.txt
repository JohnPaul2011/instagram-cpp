cmake_minimum_required(VERSION 3.10)
project(InstagramCpp)

set(CMAKE_CXX_STANDARD 17)

# Find CURL
find_package(CURL REQUIRED)

# Find JsonCpp - works with Ubuntu's libjsoncpp-dev package
find_package(JsonCpp REQUIRED)
if(NOT JsonCpp_FOUND)
    # Fallback for Ubuntu/Debian
    find_path(JsonCpp_INCLUDE_DIR NAMES json/json.h PATHS /usr/include/jsoncpp /usr/include)
    find_library(JsonCpp_LIBRARY NAMES jsoncpp PATHS /usr/lib/x86_64-linux-gnu /usr/lib)
    
    if(JsonCpp_INCLUDE_DIR AND JsonCpp_LIBRARY)
        set(JsonCpp_FOUND TRUE)
        set(JsonCpp_INCLUDE_DIRS ${JsonCpp_INCLUDE_DIR})
        set(JsonCpp_LIBRARIES ${JsonCpp_LIBRARY})
    else()
        message(FATAL_ERROR "JsonCpp not found. Install with: sudo apt-get install libjsoncpp-dev")
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
