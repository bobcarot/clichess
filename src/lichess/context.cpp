#include "context.hpp"

namespace lichess {

context::context(const std::string& player_name,
                 std::shared_ptr<spdlog::logger> log)
    : error(err::code::none), is_my_turn(false), is_game_started(false),
      is_game_finished(false), side(chess::Color::NONE),
      board(std::make_shared<chess::Board>()), log(std::move(log)),
      player_name(player_name) {
  moves.reserve(32);
}

} // namespace lichess
