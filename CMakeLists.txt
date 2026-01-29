cmake_minimum_required(VERSION 3.14)
project(muxi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(muxi
    src/auth.cpp
    src/transport.cpp
    src/server_client.cpp
    src/formation_client.cpp
    src/errors.cpp
    src/webhook.cpp
)

target_include_directories(muxi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(muxi
    PUBLIC
        nlohmann_json::nlohmann_json
        CURL::libcurl
        OpenSSL::SSL
        OpenSSL::Crypto
)

option(MUXI_BUILD_TESTS "Build tests" ON)
if(MUXI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS muxi
    EXPORT muxiTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)
