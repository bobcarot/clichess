#include <format>

#include "hook.hpp"

namespace lichess {

hook::hook(hook::map_t&& handlers, std::shared_ptr<context> ctx)
    : handlers_(std::move(handlers)), ctx_(std::move(ctx)) {}

void hook::operator()(std::string_view body) {
  if (body.empty())
    return;

  auto json = nlohmann::json::parse(body);
  if (!json.is_object())
    return fail("body is not an JSON object");

  const auto type = json.find("type");
  if (type == json.end())
    return fail("type field is missing");

  const auto name = type->get<std::string>();
  auto handler = handlers_.find(name);
  if (handler == handlers_.end())
    return fail(std::format("handler \"{}\" not found", name));

  handler->second(json);
}

void hook::fail(std::string_view msg) { ctx_->log->error(msg); }

std::shared_ptr<hook::board_stream>
hook::board_stream::spawn(std::shared_ptr<context> ctx) {
  auto stream = std::make_shared<hook::board_stream>(std::move(ctx));
  stream->init_hook();
  return stream;
}

hook::board_stream::board_stream(std::shared_ptr<context> ctx)
    : ctx_(ctx), hook_({}, ctx) {}

void hook::board_stream::init_hook() {
  hook_ = hook({{"gameFull",
                 [self = shared_from_this()](const nlohmann::json& body) {
                   try {
                     const auto white = body.find("white");
                     const auto name = white->find("id");
                     const auto state = body.find("state");
                     const auto moves = state->find("moves");
                     const auto& mvstr = moves->get<std::string>();

                     std::lock_guard<std::mutex> lock(self->ctx_->mtx);
                     if (!mvstr.empty()) {
                       const auto uci = mvstr.substr(mvstr.size() - 4, 4);
                       auto mv = chess::uci::uciToMove(*self->ctx_->board, uci);
                       self->ctx_->board->makeMove(mv);
                     }
                     auto side = (*name == self->ctx_->player_name)
                                     ? chess::Color::WHITE
                                     : chess::Color::BLACK;
                     self->ctx_->side = side;
                     self->ctx_->is_my_turn = (side == chess::Color::WHITE);
                   } catch (const std::exception& ex) {
                     self->ctx_->error.store(err::code::game_full);
                   }
                 }},
                {"gameState",
                 [self = shared_from_this()](const nlohmann::json& body) {
                   try {
                     const auto moves = body.find("moves");
                     const auto mvstr = moves->get<std::string>();

                     const auto last_space = mvstr.rfind(' ');
                     const auto at =
                         (last_space == mvstr.npos) ? 0 : last_space + 1;
                     const auto sz = mvstr.size() - at;
                     const auto uci = mvstr.substr(at, sz);
                     std::lock_guard<std::mutex> lock(self->ctx_->mtx);
                     auto board = self->ctx_->board;
                     auto mv = chess::uci::uciToMove(*board, uci);
                     self->ctx_->moves.push_back(mv);
                     auto log = self->ctx_->log;
                     auto my_last_move = self->ctx_->my_last_move.load();
                     if (mv.from() != my_last_move.from() &&
                         mv.to() != my_last_move.to()) {
                       log->info("mv ({} {}), last ({} {})", mv.from().index(),
                                 mv.to().index(), my_last_move.from().index(),
                                 my_last_move.to().index());
                       board->makeMove(mv);
                       self->ctx_->is_my_turn.store(true);
                     }
                     log->info("move {} maked", uci);
                   } catch (const std::exception& ex) {
                     self->ctx_->error.store(err::code::game_state);
                   }
                 }},
                {"chatLine",
                 [self = shared_from_this()](const nlohmann::json& body) {
                   try {
                     const auto& player = body["username"].get<std::string>();
                     const auto& text = body["text"].get<std::string>();
                     self->ctx_->log->info("[chat] {}> {}", player, text);
                   } catch (const std::exception& ex) {
                     self->ctx_->error.store(err::code::chat_line);
                   }
                 }}},
               ctx_);
}

hook hook::board_stream::get_hook() const { return hook_; }

std::shared_ptr<hook::event_stream>
hook::event_stream::spawn(std::shared_ptr<context> ctx) {
  auto stream = std::make_shared<hook::event_stream>(std::move(ctx));
  stream->init_hook();
  return stream;
}

hook::event_stream::event_stream(std::shared_ptr<context> ctx)
    : ctx_(ctx), hook_({}, std::move(ctx)) {}

void hook::event_stream::init_hook() {
  hook_ =
      hook({{"gameStart",
             [self = shared_from_this()](const nlohmann::json& body) {
               try {
                 const auto game = body.find("game");
                 const auto gameId = game->find("gameId");
                 const auto fen = game->find("fen");

                 std::lock_guard<std::mutex> lock(self->ctx_->mtx);
                 self->ctx_->game_id = gameId->get<std::string>();
                 *self->ctx_->board = chess::Board(fen->get<std::string>());
                 self->ctx_->is_game_started.store(true);
                 self->ctx_->log->info("game started with id \"{}\"",
                                       self->ctx_->game_id);
               } catch (const std::exception& ex) {
                 self->ctx_->error.store(err::code::game_start);
               }
             }},
            {"gameFinish",
             [self = shared_from_this()](const nlohmann::json& body) {
               try {
                 self->ctx_->is_game_finished.store(true);
                 const auto game = body["game"];
                 const auto winner = game["winner"];
                 const auto reason = game["status"]["name"];
                 self->ctx_->log->info(
                     "game finished with: winner {} and reason {}",
                     winner.get<std::string>(), reason.get<std::string>());
               } catch (const std::exception& ex) {
                 self->ctx_->error.store(err::code::game_finish);
               }
             }},
            {"challengeDeclined",
             [self = shared_from_this()](const nlohmann::json& body) {
               try {
                 const auto reason =
                     body["challenge"]["declineReason"].get<std::string>();
                 self->ctx_->log->error("challenge decline reason: {}", reason);
                 self->ctx_->error.store(err::code::challenge_decline);
               } catch (const std::exception& ex) {
                 self->ctx_->error.store(err::code::challenge_decline);
               }
             }}},
           ctx_);
}

hook hook::event_stream::get_hook() const { return hook_; }

} // namespace lichess
