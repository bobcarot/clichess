#include <iostream>
#include <ostream>
#include <thread>
#include <utility>

#include <frozen/unordered_map.h>

#include <chess.hpp>

#include "render.hpp"

namespace render {

static constexpr auto chess_glyphs =
    frozen::make_unordered_map<chess::Piece::underlying, std::string_view>(
        {{chess::Piece::WHITEROOK, "♜"},
         {chess::Piece::WHITEKNIGHT, "♞"},
         {chess::Piece::WHITEBISHOP, "♝"},
         {chess::Piece::WHITEQUEEN, "♛"},
         {chess::Piece::WHITEKING, "♚"},
         {chess::Piece::WHITEPAWN, "♟"},
         {chess::Piece::BLACKROOK, "♖"},
         {chess::Piece::BLACKKNIGHT, "♘"},
         {chess::Piece::BLACKBISHOP, "♗"},
         {chess::Piece::BLACKQUEEN, "♕"},
         {chess::Piece::BLACKKING, "♔"},
         {chess::Piece::BLACKPAWN, "♙"},
         {chess::Piece::NONE, "."}});

context::context(std::ostream& stream) : stream_(stream) {}

std::ostream& context::out() noexcept { return stream_; }

animation::animation(std::shared_ptr<context> out, animation::vec_t&& frames)
    : out_(std::move(out)), frames_(std::move(frames)), cur_(frames_.begin()) {}

void animation::print() const {
  out_->out() << *cur_;
  out_->out().flush();
}

void animation::restart() { cur_ = frames_.begin(); }

void animation::next() {
  if (++cur_ == frames_.end())
    restart();
}

board_renderer::board_renderer(std::shared_ptr<context> out,
                               std::weak_ptr<chess::Board> board)
    : out_(std::move(out)), board_(board) {}

void board_renderer::print(chess::Color side) const {
  auto board = board_.lock();
  std::string_view files =
      (side == chess::Color::WHITE) ? "  a b c d e f g h" : "  h g f e d c b a";
  out_->out() << files << "\n";
  for (auto row = 0; row < 8; ++row) {
    auto rown = (side == chess::Color::WHITE) ? 8 - row : row + 1;
    out_->out() << rown << " ";
    for (auto col = 0; col < 8; ++col)
      out_->out() << glyph_at(*board, square_by_side(row, col, side)) << " ";
    out_->out() << rown << "\n";
  }
  out_->out() << files << "\n";
  out_->out().flush();
}

void wait(animation& anim, std::chrono::milliseconds pause) {
  anim.print();
  anim.next();
  std::this_thread::sleep_for(pause);
}

std::string_view glyph_at(const chess::Board& board, chess::Square at) {
  const auto piece = board.at(at);
  return chess_glyphs.at(piece.internal());
}

chess::Square square_by_side(int row, int col, chess::Color side) {
  if (side == chess::Color::WHITE)
    return chess::Square(chess::File(col), chess::Rank(7 - row));
  return chess::Square(chess::File(7 - col), chess::Rank(row));
}

} // namespace render
