#pragma once

#include <functional>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "context.hpp"

namespace lichess {

class hook {
public:
  class board_stream;
  class event_stream;

  using event_handler_fn = std::function<void(const nlohmann::json&)>;
  using map_t = std::unordered_map<std::string_view, event_handler_fn>;
  hook(map_t&&, std::shared_ptr<context>);

  void operator()(std::string_view);

private:
  map_t handlers_;
  std::shared_ptr<context> ctx_;

  void fail(std::string_view msg);
}; // class hook

class hook::board_stream : public std::enable_shared_from_this<board_stream> {
public:
  static std::shared_ptr<board_stream> spawn(std::shared_ptr<context>);

  board_stream(std::shared_ptr<context>);

  void init_hook();
  hook get_hook() const;

private:
  std::shared_ptr<context> ctx_;
  hook hook_;
}; // class hook::board_stream

class hook::event_stream
    : public std::enable_shared_from_this<hook::event_stream> {
public:
  static std::shared_ptr<event_stream> spawn(std::shared_ptr<context>);

  event_stream(std::shared_ptr<context>);

  void init_hook();
  hook get_hook() const;

private:
  std::shared_ptr<context> ctx_;
  hook hook_;
}; // class hook::event_stream

} // namespace lichess
