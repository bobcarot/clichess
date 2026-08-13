add_library(lichess STATIC
  context.cpp
  hook.cpp
  request.cpp
)

target_include_directories(lichess
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${chess-library_SOURCE_DIR}/include
)

target_link_libraries(lichess
  PUBLIC
    Boost::boost
    frozen::frozen
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
