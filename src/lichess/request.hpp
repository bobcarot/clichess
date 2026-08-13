#pragma once

#include <boost/beast/http.hpp>

#include "context.hpp"

namespace beast = boost::beast;
namespace http = beast::http;

namespace lichess::request {

class get_email {
public:
  get_email(std::string_view token);

  void compile(http::request<http::string_body>&);

private:
  std::string_view token_;
}; // class get_email

class make_move {
public:
  struct error_context;

  make_move(std::string_view token, std::string_view game_id,
            std::string_view uci);

  void compile(http::request<http::string_body>&);

private:
  std::string_view token_;
  std::string_view game_id_;
  std::string_view uci_;
}; // class make_move

struct make_move::error_context {
  error_context(std::size_t max);

  const std::size_t max_attempts;
  std::size_t attempts = 0;
}; // struct make_move::error_context

class new_challenge {
public:
  struct config {
    std::string_view opponent_name;
    std::size_t limit = 300;
    std::size_t increment = 1;
    bool is_rated = false;
    bool is_color_random = true;
    chess::Color color = chess::Color::WHITE;
  }; // struct config

  new_challenge(std::string_view token, const config&);

  void compile(http::request<http::string_body>&);

private:
  std::string_view token_;
  config cfg_;
}; // class new_challenge

class open_board_stream {
public:
  open_board_stream(std::string_view token, std::string_view game_id);

  void compile(http::request<http::string_body>&);

private:
  std::string_view token_;
  std::string_view game_id_;
}; // class open_board_stream

class open_event_stream {
public:
  open_event_stream(std::string_view token);

  void compile(http::request<http::string_body>&);

private:
  std::string_view token_;
}; // class event_stream

class resign_a_game {
public:
  resign_a_game(std::string_view token, std::string_view game_id);

  void compile(http::request<http::string_body>&);

private:
  std::string_view token_;
  std::string_view game_id_;
}; // class resign_a_game

} // namespace lichess::request
