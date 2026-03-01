#include "handlers/static_handler.h"

#include "static/static_file_server.h"

namespace veloz::gateway {

StaticHandler::StaticHandler(StaticFileServer& server) : server_(server) {}

kj::Promise<void> StaticHandler::handle(kj::HttpMethod method, kj::StringPtr url,
                                        const kj::HttpHeaders& headers,
                                        kj::AsyncInputStream& requestBody,
                                        kj::HttpService::Response& response) {

  (void)requestBody; // Ignored for static files

  return server_.serve_file(method, url, headers, response);
}

} // namespace veloz::gateway
