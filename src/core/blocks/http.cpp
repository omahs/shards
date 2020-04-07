#include <exception>
#define BOOST_ERROR_CODE_HEADER_ONLY
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <taskflow/taskflow.hpp>

#include "blockwrapper.hpp"
#include "shared.hpp"

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
namespace ssl = net::ssl;       // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace chainblocks {
namespace Http {
struct Get {
  constexpr static int version = 11;

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  static inline Parameters params{
      {"Host",
       "The remote host address or IP.",
       {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Target",
       "The remote host target path to open.",
       {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Port",
       "The remote host port.",
       {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Secure", "If the connection should be secured.", {CoreInfo::BoolType}}};

  CBParametersInfo parameters() { return params; }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      host = value;
      break;

    case 1:
      target = value;
      break;
    case 2:
      port = value;
      break;
    case 3:
      ssl = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return host;
    case 1:
      return target;
    case 2:
      return port;
    case 3:
      return Var(ssl);
    default:
      return {};
    }
  }

  void connect(CBContext *context, AsyncOp<InternalCore> &op) {
    try {
      op.sidechain<tf::Taskflow>(Tasks, [&]() {
        if (ssl) {
          // Set SNI Hostname (many hosts need this to handshake
          // successfully)
          if (!SSL_set_tlsext_host_name(stream.native_handle(),
                                        host.get().payload.stringValue)) {
            beast::error_code ec{static_cast<int>(::ERR_get_error()),
                                 net::error::get_ssl_category()};
            throw beast::system_error{ec};
          }
        }

        resolved = resolver.resolve(host.get().payload.stringValue,
                                    port.get().payload.stringValue);

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(stream).connect(resolved);

        if (ssl) {
          // Perform the SSL handshake
          stream.handshake(ssl::stream_base::client);
        }

        connected = true;
      });
    } catch (std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      LOG(ERROR) << "Http connection failed, pausing chain half a second "
                    "before restart, exception: "
                 << ex.what();
      suspend(context, 0.5);
      throw ActivationError("Http connection failed, restarting chain.",
                            CBChainState::Restart, false);
    }
  }

  void request(CBContext *context, AsyncOp<InternalCore> &op) {
    try {
      op.sidechain<tf::Taskflow>(Tasks, [&]() {
        // Set up an HTTP GET request message
        http::request<http::string_body> req{
            http::verb::get, target.get().payload.stringValue, version};
        req.set(http::field::host, host.get().payload.stringValue);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // Send the HTTP request to the remote host
        http::write(stream, req);

        // Receive the HTTP response
        http::read(stream, buffer, res);
      });
    } catch (std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      LOG(ERROR) << "Http request failed, pausing chain half a second "
                    "before restart, exception: "
                 << ex.what();

      resetStream();

      suspend(context, 0.5);
      throw ActivationError("Http request failed, restarting chain.",
                            CBChainState::Restart, false);
    }
  }

  void resetStream() {
    beast::error_code ec;
    stream.shutdown(ec);
    stream = beast::ssl_stream<beast::tcp_stream>(ioc, ctx);
    connected = false;
  }

  void cleanup() { resetStream(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    AsyncOp<InternalCore> op(context);

    if (!connected)
      connect(context, op);

    buffer.clear();
    res.clear();

    request(context, op);

    return Var(res.body());
  }

private:
#if 1
  tf::Executor &Tasks{Singleton<tf::Executor>::value};
#else
  static inline tf::Executor Tasks{1};
#endif

  ParamVar port{Var("443")};
  ParamVar host{Var("www.example.com")};
  ParamVar target{Var("/")};
  bool ssl = true;

  bool connected = false;

  // The io_context is required for all I/O
  net::io_context ioc;

  // The SSL context is required, and holds certificates
  ssl::context ctx{ssl::context::tlsv12_client};

  // These objects perform our I/O
  tcp::resolver resolver{ioc};
  beast::ssl_stream<beast::tcp_stream> stream{ioc, ctx};

  tcp::resolver::results_type resolved;

  // This buffer is used for reading and must be persisted
  beast::flat_buffer buffer;

  // Declare a container to hold the response
  http::response<http::string_body> res;
};

void registerBlocks() { REGISTER_CBLOCK("Http.Get", Get); }
} // namespace Http
} // namespace chainblocks
