#pragma once

#include <chrono>
#include <memory>
#include <ostream>
#include <string_view>
#include <vector>

#include <chess.hpp>

namespace render {

class context {
public:
  context(std::ostream&);

  std::ostream& out() noexcept;

private:
  std::ostream& stream_;
}; // class context

class animation {
public:
  using vec_t = std::vector<char>;

  animation(std::shared_ptr<context>, vec_t&&);

  void print() const;
  void restart();
  void next();

private:
  mutable std::shared_ptr<context> out_;
  vec_t frames_;
  vec_t::const_iterator cur_;
}; // class animation

class board_renderer {
public:
  board_renderer(std::shared_ptr<context>, std::weak_ptr<chess::Board>);

  void print(chess::Color side) const;

private:
  mutable std::shared_ptr<context> out_;
  std::weak_ptr<chess::Board> board_;
}; // calss board_renderer

void wait(animation&, std::chrono::milliseconds);
std::string_view glyph_at(const chess::Board&, chess::Square);
chess::Square square_by_side(int row, int col, chess::Color side);

} // namespace render
