#include "gateway_server.h"

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

}

KJ_TEST("gateway responds to /api/control/health") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  veloz::gateway::GatewayServer server(*headerTable);

  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);
  kj::NullStream requestBody;
  TestResponse response;

  auto promise = server.request(kj::HttpMethod::GET, "/api/control/health"_kj,
                                kj::HttpHeaders(*headerTable), requestBody, response);
  promise.wait(waitScope);

  KJ_EXPECT(response.statusCode == 200);
}
