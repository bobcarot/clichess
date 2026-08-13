#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <spdlog/logger.h>

#include <error.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

namespace network {

class context : public std::enable_shared_from_this<context> {
public:
  context(std::string_view host, std::string_view port,
          std::shared_ptr<spdlog::logger> log);

  net::io_context& io() noexcept;
  ssl::context& ssl() noexcept;
  std::string_view host() const noexcept;
  std::string_view port() const noexcept;

  std::shared_ptr<spdlog::logger> log() noexcept;
  void run();
  void stop();
  void ssl_set_hostname(SSL*);

private:
  std::shared_ptr<spdlog::logger> log_;

  std::string_view host_;
  std::string_view port_;

  net::io_context ioc_;
  ssl::context tls_;

  net::executor_work_guard<net::io_context::executor_type> work_;
}; // class context

} // namespace network

namespace network::sync {

class session {
public:
  session(std::shared_ptr<context>);

  template <typename Req> void operator<<(Req&&);
  void operator>>(std::string&);

private:
  std::shared_ptr<context> ctx_;
  beast::ssl_stream<beast::tcp_stream> stream_;
  beast::flat_buffer buffer_;
}; // class session

template <typename Req> void session::operator<<(Req&& r) {
  http::request<http::string_body> req;
  r.compile(req);
  http::write(stream_, req);
}

} // namespace network::sync

namespace network::async {

template <typename Derived>
class session_base
    : public std::enable_shared_from_this<session_base<Derived>> {
public:
  template <typename Cb> session_base(std::shared_ptr<context>, Cb&& hook);
  virtual ~session_base() = default;

  void start();

protected:
  std::shared_ptr<context> ctx_;
  std::function<void(std::string_view)> hook_;
  http::request<http::string_body> request_;
  tcp::resolver resolver_;
  beast::flat_buffer buffer_;
  beast::ssl_stream<beast::tcp_stream> stream_;

  std::shared_ptr<Derived> self();
  void fail(beast::error_code ec, char const* what, err::code);
  void shutdown();

private:
  void on_resolve(beast::error_code, tcp::resolver::results_type);
  void on_connect(beast::error_code,
                  tcp::resolver::results_type::endpoint_type);
  void on_handshake(beast::error_code);
  void on_shutdown(beast::error_code);
}; // class session_base

class session final : public session_base<session> {
public:
  template <typename Cb> session(std::shared_ptr<context>, Cb&& hook);

  template <typename Req> void operator<<(Req&&);

private:
  friend class session_base<session>;

  http::response<http::string_body> response_;

  void on_write(beast::error_code, std::size_t);
  void on_read(beast::error_code, std::size_t);
}; // class session

class stream_session final : public session_base<stream_session> {
public:
  template <typename Cb> stream_session(std::shared_ptr<context>, Cb&& hook);

  template <typename Req> void operator<<(Req&&);

private:
  friend class session_base<stream_session>;

  std::array<char, 8192> chunk_buffer_;
  http::response_parser<http::buffer_body> parser_;
  std::string accum_;

  void on_write(beast::error_code, std::size_t);
  void on_read_header(beast::error_code, std::size_t);
  void on_read_body(beast::error_code, std::size_t);

  void prepare_buffer();
  void process_chunk();
  void read_body();
}; // class stream_session

template <typename Derived>
template <typename Cb>
session_base<Derived>::session_base(std::shared_ptr<context> ctx, Cb&& hook)
    : ctx_(std::move(ctx)), hook_(std::forward<Cb>(hook)),
      resolver_(ctx_->io()), stream_(ctx_->io(), ctx_->ssl()) {
  ctx_->ssl_set_hostname(stream_.native_handle());
}

template <typename Derived>
std::shared_ptr<Derived> session_base<Derived>::self() {
  return std::static_pointer_cast<Derived>(this->shared_from_this());
}
template <typename Derived>
void session_base<Derived>::fail(beast::error_code ec, char const* what,
                                 err::code err) {
  ctx_->log()->error("{}", err::message(err));
  ctx_->log()->error(std::format("[internal] {}: {}", what, ec.message()));
  shutdown();
}

template <typename Derived> void session_base<Derived>::shutdown() {
  stream_.async_shutdown(
      beast::bind_front_handler(&session_base::on_shutdown, self()));
}

template <typename Derived> void session_base<Derived>::start() {
  resolver_.async_resolve(
      ctx_->host(), ctx_->port(),
      beast::bind_front_handler(&session_base::on_resolve, self()));
}

template <typename Derived>
void session_base<Derived>::on_resolve(beast::error_code ec,
                                       tcp::resolver::results_type results) {
  if (ec)
    return fail(ec, "resolve", err::code::session_resolve);

  using namespace std::literals;
  beast::get_lowest_layer(stream_).async_connect(
      results, beast::bind_front_handler(&session_base::on_connect, self()));
}

template <typename Derived>
void session_base<Derived>::on_connect(
    beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
  if (ec)
    return fail(ec, "connect", err::code::session_connect);

  stream_.async_handshake(
      ssl::stream_base::client,
      beast::bind_front_handler(&session_base::on_handshake, self()));
}

template <typename Derived>
void session_base<Derived>::on_handshake(beast::error_code ec) {
  if (ec)
    return fail(ec, "handshake", err::code::session_handshake);

  using namespace std::literals;
  http::async_write(stream_, request_,
                    beast::bind_front_handler(&Derived::on_write, self()));
}

template <typename Derived>
void session_base<Derived>::on_shutdown(beast::error_code ec) {
  if (ec)
    ctx_->log()->info("session shutdown: {}", ec.what());
  else
    ctx_->log()->info("session closed");
}

template <typename Cb>
session::session(std::shared_ptr<context> ctx, Cb&& hook)
    : session_base(std::move(ctx), std::forward<Cb>(hook)) {}

template <typename Req> void session::operator<<(Req&& r) {
  r.compile(request_);
  start();
  ctx_->log()->debug("session started");
}

template <typename Cb>
stream_session::stream_session(std::shared_ptr<context> ctx, Cb&& hook)
    : session_base(std::move(ctx), std::forward<Cb>(hook)) {
  accum_.reserve(chunk_buffer_.size());
}

template <typename Req> void stream_session::operator<<(Req&& r) {
  r.compile(request_);
  start();
  ctx_->log()->debug("stream session started");
}

} // namespace network::async
