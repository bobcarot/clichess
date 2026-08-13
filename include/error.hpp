#pragma once

#include <cstdint>
#include <string_view>

#include <frozen/unordered_map.h>

namespace err {

enum class code : std::uint8_t {
  none,

  // Lichess
  challenge_decline,
  game_start,
  game_state,
  game_full,
  game_finish,
  chat_line,

  // Network
  session_resolve,
  session_connect,
  session_handshake,
  session_write,
  session_read,
  session_read_header,
  session_read_body
}; // enum class code

static constexpr frozen::unordered_map<code, std::string_view, 14> strings = {
    {code::none, "success"},

    {code::challenge_decline, "challenge declined"},
    {code::game_start, "game start"},
    {code::game_state, "game state"},
    {code::game_full, "game full"},
    {code::game_finish, "game finish"},
    {code::chat_line, "chat line"},

    {code::session_resolve, "session resolve"},
    {code::session_connect, "session connect"},
    {code::session_handshake, "session handshake"},
    {code::session_write, "session write"},
    {code::session_read, "session read"},
    {code::session_read_header, "session read header"},
    {code::session_read_body, "session read body"}};

constexpr std::string_view message(code ec) noexcept {
  const auto msg = strings.find(ec);
  return msg->second;
}

} // namespace err
