#include "network.hpp"

namespace network {

context::context(std::string_view host, std::string_view port,
                 std::shared_ptr<spdlog::logger> log)
    : log_(std::move(log)), host_(host), port_(port),
      tls_(ssl::context::tlsv13_client), work_(net::make_work_guard(ioc_)) {
  tls_.set_default_verify_paths();
  tls_.set_verify_mode(ssl::verify_peer);
}

net::io_context& context::io() noexcept { return ioc_; }

ssl::context& context::ssl() noexcept { return tls_; }

std::string_view context::host() const noexcept { return host_; }

std::string_view context::port() const noexcept { return port_; }

std::shared_ptr<spdlog::logger> context::log() noexcept { return log_; }

void context::run() { ioc_.run(); }

void context::stop() { ioc_.stop(); }

void context::ssl_set_hostname(SSL* ssl) {
  if (!SSL_set_tlsext_host_name(ssl, host_.data()))
    throw beast::system_error(static_cast<int>(::ERR_get_error()),
                              net::error::get_ssl_category());
}

} // namespace network

namespace network::sync {

session::session(std::shared_ptr<context> ctx)
    : ctx_(std::move(ctx)), stream_(ctx_->io(), ctx_->ssl()), buffer_() {
  ctx_->ssl_set_hostname(stream_.native_handle());

  stream_.set_verify_callback(ssl::host_name_verification(ctx_->host().data()));

  tcp::resolver resolver(ctx_->io());
  const auto results = resolver.resolve(ctx_->host(), ctx_->port());
  beast::get_lowest_layer(stream_).connect(results);

  stream_.handshake(ssl::stream_base::client);
}

void session::operator>>(std::string& r) {
  http::response<http::string_body> res;
  http::read(stream_, buffer_, res);
  r = res.body();
}

} // namespace network::sync

namespace network::async {

void session::on_write(beast::error_code ec, std::size_t) {
  if (ec)
    return fail(ec, "session::on_write", err::code::session_write);

  response_ = {};
  buffer_.consume(buffer_.size());

  http::async_read(stream_, buffer_, response_,
                   beast::bind_front_handler(&session::on_read, self()));
}

void session::on_read(beast::error_code ec, std::size_t) {
  if (ec)
    return fail(ec, "session::on_read", err::code::session_read);

  hook_(response_.body().data());
}

void stream_session::on_write(beast::error_code ec, std::size_t) {
  if (ec)
    return fail(ec, "stream_session::on_write", err::code::session_write);

  prepare_buffer();
  http::async_read_header(
      stream_, buffer_, parser_,
      beast::bind_front_handler(&stream_session::on_read_header, self()));
}

void stream_session::on_read_header(beast::error_code ec, std::size_t) {
  if (ec)
    return fail(ec, "stream_session::on_read_header",
                err::code::session_read_header);

  if (parser_.get().result() != http::status::ok) {
    ctx_->log()->error("bad status code {}", parser_.get().result_int());
    return;
  }
  read_body();
}

void stream_session::on_read_body(beast::error_code ec, std::size_t) {
  if (ec && ec == http::error::need_buffer)
    ec = {};

  if (ec && ec == http::error::end_of_stream) {
    process_chunk();
    shutdown();
    return;
  }

  if (ec)
    return fail(ec, ec.what().c_str(), err::code::session_read_body);

  process_chunk();

  if (parser_.is_done())
    return;

  read_body();
}

void stream_session::prepare_buffer() {
  auto& body = parser_.get().body();
  body.data = chunk_buffer_.data();
  body.size = chunk_buffer_.size();
}

void stream_session::process_chunk() {
  auto& body = parser_.get().body();
  auto written = chunk_buffer_.size() - body.size;
  accum_.append(std::string_view(chunk_buffer_.data(), written));
  auto nlpos = accum_.find('\n');
  while (nlpos != accum_.npos) {
    auto event = std::string_view(accum_.data(), nlpos);
    hook_(event);
    accum_.erase(0, nlpos + 1);
    nlpos = accum_.find('\n');
  }
}

void stream_session::read_body() {
  prepare_buffer();
  http::async_read_some(
      stream_, buffer_, parser_,
      beast::bind_front_handler(&stream_session::on_read_body, self()));
}

} // namespace network::async
