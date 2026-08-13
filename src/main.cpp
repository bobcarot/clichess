#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <unistd.h>

#include <chess.hpp>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <error.hpp>

#include "lichess/context.hpp"
#include "lichess/hook.hpp"
#include "lichess/request.hpp"
#include "network.hpp"
#include "render.hpp"

using namespace std::literals;

namespace rng = std::ranges;

std::atomic<bool> is_running = true;

void exit_handler(int) {
  is_running.store(false);
  close(STDIN_FILENO);
}

std::shared_ptr<spdlog::logger> spawn_logger() {
  // Disable default global logger
  spdlog::set_level(spdlog::level::off);

  auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  sink->set_level(spdlog::level::info);
  auto logger = std::make_shared<spdlog::logger>("cli-chess",
                                                 spdlog::sinks_init_list{sink});
  logger->set_level(spdlog::level::info);
  return logger;
}

std::string load_token(std::filesystem::path path) {
  std::ifstream config(path);
  if (!config.is_open())
    throw std::runtime_error(std::format("config {} not opened", path.c_str()));

  std::string token;
  config >> token;
  return token;
}

void start_event_stream(std::shared_ptr<network::context> net,
                        std::shared_ptr<lichess::context> game,
                        std::string_view tok) {
  auto session = std::make_shared<network::async::stream_session>(
      net, lichess::hook::event_stream::spawn(game)->get_hook());
  *session << lichess::request::open_event_stream(tok);
}

void send_challenge(std::shared_ptr<network::context> net, std::string_view tok,
                    const lichess::request::new_challenge::config& cfg) {
  auto session = std::make_shared<network::async::session>(net, [](auto) {});
  *session << lichess::request::new_challenge(tok, cfg);
}

void start_board_stream(std::shared_ptr<network::context> net,
                        std::shared_ptr<lichess::context> game,
                        std::string_view tok, std::string_view gameid) {
  auto session = std::make_shared<network::async::stream_session>(
      net, lichess::hook::board_stream::spawn(game)->get_hook());
  *session << lichess::request::open_board_stream(tok, gameid);
}

void resign(std::shared_ptr<network::context> net, std::string_view tok,
            std::string_view gameid) {
  auto session = std::make_shared<network::async::session>(net, [](auto) {});
  *session << lichess::request::resign_a_game(tok, gameid);
}

void speak_until_wait(std::string_view msg, render::animation& anim) {
  std::cout << msg << "...";
  render::wait(anim, 100ms);
  std::cout.flush();
  std::system("clear");
}

void print_results(std::shared_ptr<lichess::context> game) {
  std::cout << "moves: (";
  rng::for_each(game->moves, [](const auto& mv) {
    std::cout << chess::uci::moveToUci(mv) << " ";
  });
  std::cout << "\b)" << std::endl;
  std::cout << "Bye!" << std::endl;
}

int main(int argc, char* argv[]) {
  if (argc == 1) {
    std::cout << "Usage: " << argv[0] << " <opponent>" << std::endl;
    return -1;
  }

  std::signal(SIGINT, exit_handler);
  std::signal(SIGTERM, exit_handler);

  auto log = spawn_logger();
  auto net =
      std::make_shared<network::context>(lichess::host, lichess::port, log);
  std::thread io_task([net]() { net->io().run(); });
  auto game_tok = load_token("game-token");
  auto game = std::make_shared<lichess::context>("deadcm", log);

  start_event_stream(net, game, game_tok);
  send_challenge(
      net, game_tok,
      lichess::request::new_challenge::config{.opponent_name = argv[1],
                                              .is_color_random = false,
                                              .color = chess::Color::WHITE});

  auto canvas = std::make_shared<render::context>(std::cout);

  render::animation mill(canvas, {'|', '/', '-', '\\'});

  while (!game->is_game_started.load()) {
    speak_until_wait("Accepting challenge", mill);
    auto err = game->error.load();
    if (err != err::code::none) {
      log->error("on {}", err::message(err));
      is_running.store(false);
    }
    if (!is_running.load())
      break;
  }

  if (is_running.load()) {
    start_board_stream(net, game, game_tok, game->game_id);

    render::board_renderer paint(canvas, game->board);
    std::string uci('\0', 4);
    chess::Movelist legals;
    while (is_running.load()) {
      if (game->is_my_turn.load()) {
        {
          std::lock_guard<std::mutex> lock(game->mtx);
          chess::movegen::legalmoves(legals, *game->board);
        }
        chess::Move mv;
        for (;;) {
          std::system("clear");
          paint.print(game->side);
          std::cout << "> ";
          std::cin >> uci;
          if (!is_running.load())
            break;
          {
            std::lock_guard<std::mutex> lock(game->mtx);
            mv = chess::uci::uciToMove(*game->board, uci);
          }
          if (rng::find(legals, mv) != legals.end())
            break;
          log->error("move {} is illegal", uci);
        }
        std::lock_guard<std::mutex> lock(game->mtx);
        game->my_last_move.store(mv);
        game->is_my_turn.store(false);
        game->board->makeMove(mv);
        std::system("clear");
        paint.print(game->side);
        auto session =
            std::make_shared<network::async::session>(net, [](auto) {});
        *session << lichess::request::make_move(game_tok, game->game_id, uci);
      }
      while (!game->is_my_turn.load()) {
        paint.print(game->side);
        speak_until_wait("Waiting", mill);
        auto err = game->error.load();
        if (err != err::code::none) {
          log->error("on {}", err::message(err));
          is_running.store(false);
        }
        if (!is_running.load())
          break;
      }
      if (!is_running.load())
        break;
    }

    if (!is_running.load())
      resign(net, game_tok, game->game_id);
  }

  io_task.join();
  net->stop();

  print_results(game);
  return 0;
}
