#pragma once

#include <kj/async-io.h>
#include <kj/compat/http.h>

namespace veloz::gateway {

class GatewayServer final : public kj::HttpService {
public:
  explicit GatewayServer(const kj::HttpHeaderTable& headerTable);

  kj::Promise<void> request(kj::HttpMethod method, kj::StringPtr url,
                            const kj::HttpHeaders& headers, kj::AsyncInputStream& requestBody,
                            Response& response) override;

private:
  const kj::HttpHeaderTable& headerTable_;
};

} // namespace veloz::gateway
