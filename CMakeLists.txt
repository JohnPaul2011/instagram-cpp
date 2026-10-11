cmake_minimum_required(VERSION 3.10)
project(InstagramCpp)

set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)
find_path(JsonCpp_INCLUDE_DIR NAMES json/json.h PATHS /usr/include/jsoncpp)
find_library(JsonCpp_LIBRARY NAMES jsoncpp PATHS /usr/lib/x86_64-linux-gnu)

add_executable(instagram
    instagram.cpp
    main.cpp          # Add this line
)

target_link_libraries(instagram
    ${CURL_LIBRARIES}
    ${JsonCpp_LIBRARY}
)

target_include_directories(instagram PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${JsonCpp_INCLUDE_DIR}
)
