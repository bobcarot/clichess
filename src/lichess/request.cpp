#include "request.hpp"

namespace lichess::request {

get_email::get_email(std::string_view token) : token_(token) {}

void get_email::compile(http::request<http::string_body>& req) {
  req.method(http::verb::get);
  req.target("/api/account/email");
  req.version(11);
  req.set(http::field::host, host);
  req.set(http::field::authorization, std::format("Bearer {}", token_));
}

make_move::make_move(std::string_view token, std::string_view game_id,
                     std::string_view uci)
    : token_(token), game_id_(game_id), uci_(uci) {}

void make_move::compile(http::request<http::string_body>& req) {
  req.method(http::verb::post);
  req.target(std::format("/api/board/game/{}/move/{}", game_id_, uci_));
  req.version(11);
  req.set(http::field::host, host);
  req.set(http::field::authorization, std::format("Bearer {}", token_));
  req.prepare_payload();
}

make_move::error_context::error_context(std::size_t max)
    : max_attempts(max), attempts(0) {}

new_challenge::new_challenge(std::string_view token,
                             const new_challenge::config& cfg)
    : token_(token), cfg_(cfg) {}

void new_challenge::compile(http::request<http::string_body>& req) {
  req.method(http::verb::post);
  req.target(std::format("/api/challenge/{}", cfg_.opponent_name));
  req.version(11);
  req.set(http::field::host, host);
  req.set(http::field::content_type, "application/x-www-form-urlencoded");
  req.set(http::field::authorization, std::format("Bearer {}", token_));

  req.body() = std::format(
      "clock.limit={}&clock.increment={}&rated={}&color={}&variant=standard",
      cfg_.limit, cfg_.increment, cfg_.is_rated,
      cfg_.is_color_random                  ? "random"
      : (cfg_.color == chess::Color::WHITE) ? "white"
                                            : "black");
  req.prepare_payload();
}

open_board_stream::open_board_stream(std::string_view token,
                                     std::string_view game_id)
    : token_(token), game_id_(game_id) {}

void open_board_stream::compile(http::request<http::string_body>& req) {
  req.method(http::verb::get);
  req.target(std::format("/api/board/game/stream/{}", game_id_));
  req.version(11);
  req.set(http::field::host, host);
  req.set(http::field::authorization, std::format("Bearer {}", token_));
}

open_event_stream::open_event_stream(std::string_view token) : token_(token) {}

void open_event_stream::compile(http::request<http::string_body>& req) {
  req.method(http::verb::get);
  req.target("/api/stream/event");
  req.version(11);
  req.set(http::field::host, host);
  req.set(http::field::authorization, std::format("Bearer {}", token_));
}

resign_a_game::resign_a_game(std::string_view token, std::string_view game_id)
    : token_(token), game_id_(game_id) {}

void resign_a_game::compile(http::request<http::string_body>& req) {
  req.method(http::verb::post);
  req.target(std::format("/api/board/game/{}/resign", game_id_));
  req.version(11);
  req.set(http::field::host, host);
  req.set(http::field::authorization, std::format("Bearer {}", token_));
  req.prepare_payload();
}

} // namespace lichess::request
