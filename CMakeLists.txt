cmake_minimum_required(VERSION 3.10)
project(InstagramCpp)

set(CMAKE_CXX_STANDARD 17)

# Force static linking
set(BUILD_SHARED_LIBS OFF)
set(CURL_USE_STATIC_LIBS ON)

# Find CURL
find_package(CURL REQUIRED)

# Find JsonCpp - multiple fallback strategies
find_package(JsonCpp 1.9.4 CONFIG)
if(NOT JsonCpp_FOUND)
    find_package(JsonCpp CONFIG)
endif()

if(NOT JsonCpp_FOUND)
    # Fallback for Ubuntu/Debian system packages
    find_path(JsonCpp_INCLUDE_DIR NAMES json/json.h
        PATHS /usr/include/jsoncpp /usr/local/include/jsoncpp
    )
    find_library(JsonCpp_LIBRARY NAMES jsoncpp
        PATHS /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib
    )

    if(JsonCpp_INCLUDE_DIR AND JsonCpp_LIBRARY)
        set(JsonCpp_FOUND TRUE)
        message(STATUS "Found JsonCpp: include=${JsonCpp_INCLUDE_DIR}, lib=${JsonCpp_LIBRARY}")
    else()
        message(FATAL_ERROR "JsonCpp not found. Install with: sudo apt-get install libjsoncpp-dev")
    endif()
endif()

# Add executable
add_executable(instagram
    instagram.cpp
    main.cpp
)

# Link libraries - handle both CONFIG and fallback cases
if(JsonCpp_FOUND AND TARGET JsonCpp::JsonCpp)
    target_link_libraries(instagram PRIVATE CURL::libcurl JsonCpp::JsonCpp)
else()
    target_link_libraries(instagram PRIVATE ${CURL_LIBRARIES} ${JsonCpp_LIBRARY})
endif()

# Include directories
if(TARGET JsonCpp::JsonCpp)
    target_include_directories(instagram PRIVATE ${CURL_INCLUDE_DIRS})
else()
    target_include_directories(instagram PRIVATE ${CURL_INCLUDE_DIRS} ${JsonCpp_INCLUDE_DIR})
endif()
