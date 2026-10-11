cmake_minimum_required(VERSION 3.10)
project(InstagramCpp)

set(CMAKE_CXX_STANDARD 17)

# Find CURL
find_package(CURL REQUIRED)

# Find JsonCpp - Works with Ubuntu's libjsoncpp-dev
find_path(JsonCpp_INCLUDE_DIR NAMES json/json.h PATHS /usr/include/jsoncpp /usr/local/include/jsoncpp)
find_library(JsonCpp_LIBRARY NAMES jsoncpp PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)

if(NOT JsonCpp_INCLUDE_DIR OR NOT JsonCpp_LIBRARY)
    message(FATAL_ERROR "JsonCpp not found. Install with: sudo apt-get install libjsoncpp-dev")
endif()

# Add executable
add_executable(instagram
    instagram.cpp
)

# Link libraries
target_link_libraries(instagram
    ${CURL_LIBRARIES}
    ${JsonCpp_LIBRARY}
)

# Include directories
target_include_directories(instagram PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${JsonCpp_INCLUDE_DIR}
)
