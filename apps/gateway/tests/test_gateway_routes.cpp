#include "gateway_server.h"
#include "router.h"

#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/test.h>

namespace {

class TestResponse final : public kj::HttpService::Response {
public:
  kj::uint statusCode = 0;
  kj::String statusText = kj::str("");

  kj::Own<kj::AsyncOutputStream> send(kj::uint statusCode, kj::StringPtr statusText,
                                      const kj::HttpHeaders& headers,
                                      kj::Maybe<uint64_t> expectedBodySize) override {
    this->statusCode = statusCode;
    this->statusText = kj::str(statusText);
    this->expectedBodySize = expectedBodySize;
    return kj::heap<kj::NullStream>();
  }

  kj::Own<kj::WebSocket> acceptWebSocket(const kj::HttpHeaders& headers) override {
    KJ_FAIL_REQUIRE("websocket not supported");
  }

  kj::Maybe<uint64_t> expectedBodySize;
};

} // namespace

KJ_TEST("gateway returns 404 for unknown route") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  veloz::gateway::Router router;
  veloz::gateway::GatewayServer server(*headerTable, router);

  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);
  kj::NullStream requestBody;
  TestResponse response;

  auto promise = server.request(kj::HttpMethod::GET, "/api/control/health"_kj,
                                kj::HttpHeaders(*headerTable), requestBody, response);
  promise.wait(waitScope);

  // With no routes registered, should return 404
  KJ_EXPECT(response.statusCode == 404);
}

KJ_TEST("gateway routes to registered handler") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  veloz::gateway::Router router;

  // Register a health check handler
  router.add_route(kj::HttpMethod::GET, "/api/control/health",
                   [&headerTable](veloz::gateway::RequestContext& ctx) -> kj::Promise<void> {
                     kj::HttpHeaders responseHeaders(*headerTable);
                     responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);
                     auto body = kj::str("{\"ok\":true}");
                     auto stream = ctx.response.send(200, "OK"_kj, responseHeaders, body.size());
                     auto writePromise = stream->write(body.asBytes());
                     return writePromise.attach(kj::mv(stream), kj::mv(body));
                   });

  veloz::gateway::GatewayServer server(*headerTable, router);

  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);
  kj::NullStream requestBody;
  TestResponse response;

  auto promise = server.request(kj::HttpMethod::GET, "/api/control/health"_kj,
                                kj::HttpHeaders(*headerTable), requestBody, response);
  promise.wait(waitScope);

  KJ_EXPECT(response.statusCode == 200);
}

KJ_TEST("gateway returns 405 for wrong method") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  veloz::gateway::Router router;

  // Register only GET for this path
  router.add_route(kj::HttpMethod::GET, "/api/orders",
                   [&headerTable](veloz::gateway::RequestContext& ctx) -> kj::Promise<void> {
                     kj::HttpHeaders responseHeaders(*headerTable);
                     auto body = kj::str("[]");
                     auto stream = ctx.response.send(200, "OK"_kj, responseHeaders, body.size());
                     auto writePromise = stream->write(body.asBytes());
                     return writePromise.attach(kj::mv(stream), kj::mv(body));
                   });

  veloz::gateway::GatewayServer server(*headerTable, router);

  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);
  kj::NullStream requestBody;
  TestResponse response;

  // Try POST on a GET-only route
  auto promise = server.request(kj::HttpMethod::POST, "/api/orders"_kj,
                                kj::HttpHeaders(*headerTable), requestBody, response);
  promise.wait(waitScope);

  KJ_EXPECT(response.statusCode == 405);
}