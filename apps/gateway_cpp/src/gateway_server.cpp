#include "gateway_server.h"

#include <kj/string.h>

namespace veloz::gateway {

GatewayServer::GatewayServer(const kj::HttpHeaderTable& headerTable)
    : headerTable_(headerTable) {}

kj::Promise<void> GatewayServer::request(kj::HttpMethod method, kj::StringPtr url,
                                         const kj::HttpHeaders& headers,
                                         kj::AsyncInputStream& requestBody,
                                         Response& response) {
  if (method == kj::HttpMethod::GET && url == "/api/control/health"_kj) {
    kj::HttpHeaders responseHeaders(headerTable_);
    responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);
    auto body = kj::str("{\"ok\":true}");
    auto stream = response.send(200, "OK"_kj, responseHeaders, body.size());
    auto writePromise = stream->write(body.asBytes());
    return writePromise.attach(kj::mv(stream), kj::mv(body));
  }

  return response.sendError(404, "Not Found"_kj, headerTable_);
}

} // namespace veloz::gateway
