#pragma once

#include <atomic>
#include <error.h>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <chess.hpp>
#include <spdlog/logger.h>

#include <error.hpp>

namespace lichess {

constexpr std::string_view host = "lichess.org";
constexpr std::string_view port = "443";

struct context {
  context(const std::string& player_name, std::shared_ptr<spdlog::logger>);

  std::mutex mtx;
  std::atomic<err::code> error;
  std::atomic<bool> is_my_turn;
  std::atomic<bool> is_game_started;
  std::atomic<bool> is_game_finished;
  chess::Color side;
  std::shared_ptr<chess::Board> board;
  std::atomic<chess::Move> my_last_move;
  std::vector<chess::Move> moves;
  std::shared_ptr<spdlog::logger> log;
  std::string game_id;
  std::string player_name;
}; // struct context

} // namespace lichess
