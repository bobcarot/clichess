cmake_minimum_required(VERSION 3.28)

project(clichess VERSION 0.9.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GITHUB https://github.com)

include(FetchContent)

find_package(Boost CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

FetchContent_Declare(
    chess-library
    GIT_REPOSITORY ${GITHUB}/Disservin/chess-library
    GIT_TAG master
)

FetchContent_Declare(
    frozen
    GIT_REPOSITORY ${GITHUB}/serge-sans-paille/frozen
    GIT_TAG 1.2.0
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY ${GITHUB}/nlohmann/json
    GIT_TAG v3.12.0
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY ${GITHUB}/gabime/spdlog
    GIT_TAG v1.17.0
)

FetchContent_MakeAvailable(
    chess-library
    frozen
    nlohmann_json
    spdlog
)

add_subdirectory(src/lichess)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/network.cpp
    src/render.cpp
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${chess-library_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        lichess

        Boost::boost
        frozen::frozen
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        spdlog::spdlog
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        $<$<CONFIG:Debug>:-g -O0 -Wall -Wextra>
        $<$<CONFIG:Release>:-O3 -Wall -Wextra -Werror>
)
