#include "static/static_file_server.h"
#include "test_common.h"

#include <cstdlib>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/filesystem.h>
#include <kj/test.h>
namespace veloz::gateway {

namespace {

// Helper to create a temporary directory with test files
class TempDir {
public:
  TempDir() : tempPath_(nullptr) {
    auto fs = kj::newDiskFilesystem();
    auto basePath = fs->getCurrentPath().evalNative("/tmp");
    auto uniqueName = kj::str("veloz_static_test_", std::rand());
    tempPath_ = basePath.append(uniqueName);

    auto nativePath = tempPath_.toNativeString(true);
    auto cmd = kj::str("rm -rf '", nativePath, "'");
    (void)std::system(cmd.cStr());
    dir_ =
        fs->getRoot().openSubdir(tempPath_, kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT);
  }

  ~TempDir() {
    // Cleanup (best effort)
    try {
      auto fs = kj::newDiskFilesystem();
      // Note: KJ doesn't have recursive delete, so we do our best
    } catch (...) {
    }
  }

  kj::PathPtr path() const {
    return tempPath_;
  }
  kj::String nativePath() const {
    auto fs = kj::newDiskFilesystem();
    return tempPath_.toNativeString(true);
  }

  void writeFile(kj::StringPtr name, kj::StringPtr content) {
    auto path = kj::Path::parse(name);
    auto file = dir_->openFile(path, kj::WriteMode::CREATE | kj::WriteMode::MODIFY);
    file->writeAll(content);
  }

private:
  kj::Path tempPath_;
  kj::Own<const kj::Directory> dir_;
};

kj::Maybe<kj::StringPtr> find_header(const kj::HttpHeaders& headers, kj::StringPtr name) {
  kj::Maybe<kj::StringPtr> result = kj::none;
  headers.forEach([&](kj::StringPtr headerName, kj::StringPtr headerValue) {
    if (headerName == name) {
      result = headerValue;
    }
  });
  return result;
}

} // namespace

// Test basic file serving
KJ_TEST("StaticFileServer: serve basic HTML file") {
  test::TestContext ctx;
  auto& headerTable = ctx.headerTable();

  // Create temp directory with test file
  TempDir tempDir;
  tempDir.writeFile("index.html", "<html><body>Hello World</body></html>"_kj);

  // Create server config
  StaticFileServer::Config config;
  config.staticDir = tempDir.nativePath();

  // Create server
  StaticFileServer server(config);

  // Create mock request
  kj::HttpHeaders headers(headerTable);
  test::MockHttpResponse response(headerTable);

  // Serve index.html
  auto promise = server.serve_file(kj::HttpMethod::GET, "/index.html", headers, response);
  promise.wait(ctx.waitScope());

  // Check response
  KJ_EXPECT(response.statusCode == 200);
  KJ_EXPECT(response.statusText == "OK");
  // Check content type
  KJ_IF_SOME(ct, response.responseHeaders.get(kj::HttpHeaderId::CONTENT_TYPE)) {
    KJ_EXPECT(ct == "text/html; charset=utf-8");
  }
  else {
    KJ_FAIL_ASSERT("Expected Content-Type header");
  }
}

// Test MIME type detection
KJ_TEST("StaticFileServer: MIME type detection") {
  test::TestContext ctx;
  auto& headerTable = ctx.headerTable();

  TempDir tempDir;
  tempDir.writeFile("script.js", "console.log('hello')"_kj);
  tempDir.writeFile("style.css", "body { margin: 0; }"_kj);
  tempDir.writeFile("data.json", R"({"key": "value"})"_kj);
  tempDir.writeFile("image.svg", "<svg></svg>"_kj);

  StaticFileServer::Config config;
  config.staticDir = tempDir.nativePath();
  StaticFileServer server(config);

  kj::HttpHeaders headers(headerTable);

  // Test JavaScript
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/script.js", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 200);
    KJ_IF_SOME(ct, response.responseHeaders.get(kj::HttpHeaderId::CONTENT_TYPE)) {
      KJ_EXPECT(ct == "application/javascript; charset=utf-8");
    }
  }

  // Test CSS
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/style.css", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 200);
    KJ_IF_SOME(ct, response.responseHeaders.get(kj::HttpHeaderId::CONTENT_TYPE)) {
      KJ_EXPECT(ct == "text/css; charset=utf-8");
    }
  }

  // Test JSON
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/data.json", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 200);
    KJ_IF_SOME(ct, response.responseHeaders.get(kj::HttpHeaderId::CONTENT_TYPE)) {
      KJ_EXPECT(ct == "application/json; charset=utf-8");
    }
  }

  // Test SVG
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/image.svg", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 200);
    KJ_IF_SOME(ct, response.responseHeaders.get(kj::HttpHeaderId::CONTENT_TYPE)) {
      KJ_EXPECT(ct == "image/svg+xml");
    }
  }
}

// Test path traversal prevention
KJ_TEST("StaticFileServer: prevent path traversal") {
  test::TestContext ctx;
  auto& headerTable = ctx.headerTable();

  TempDir tempDir;
  tempDir.writeFile("safe.txt", "safe content"_kj);

  StaticFileServer::Config config;
  config.staticDir = tempDir.nativePath();
  StaticFileServer server(config);

  kj::HttpHeaders headers(headerTable);

  // Test path traversal attempts
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/../etc/passwd", headers, response)
        .wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 403); // Forbidden
  }

  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/foo/../../etc/passwd", headers, response)
        .wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 403); // Forbidden
  }

  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/..%2F..%2Fetc%2Fpasswd", headers, response)
        .wait(ctx.waitScope());
    // This should either be 403 or 404 (not found)
    KJ_EXPECT(response.statusCode == 403 || response.statusCode == 404);
  }
}

// Test SPA routing
KJ_TEST("StaticFileServer: SPA routing fallback") {
  test::TestContext ctx;
  auto& headerTable = ctx.headerTable();

  TempDir tempDir;
  tempDir.writeFile("index.html", "<html><body>SPA App</body></html>"_kj);
  tempDir.writeFile("app.js", "console.log('app')"_kj);

  StaticFileServer::Config config;
  config.staticDir = tempDir.nativePath();
  StaticFileServer server(config);

  kj::HttpHeaders headers(headerTable);

  // Request for SPA route (no extension) should serve index.html
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/dashboard", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 200);
    KJ_IF_SOME(ct, response.responseHeaders.get(kj::HttpHeaderId::CONTENT_TYPE)) {
      KJ_EXPECT(ct == "text/html; charset=utf-8");
    }
  }

  // Request for nested SPA route should also serve index.html
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/users/123/profile", headers, response)
        .wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 200);
    KJ_IF_SOME(ct, response.responseHeaders.get(kj::HttpHeaderId::CONTENT_TYPE)) {
      KJ_EXPECT(ct == "text/html; charset=utf-8");
    }
  }

  // Request for actual file should serve that file
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/app.js", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 200);
    KJ_IF_SOME(ct, response.responseHeaders.get(kj::HttpHeaderId::CONTENT_TYPE)) {
      KJ_EXPECT(ct == "application/javascript; charset=utf-8");
    }
  }

  // Request for non-existent file with extension should return 404
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/nonexistent.css", headers, response)
        .wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 404);
  }
}

// Test index.html serving
KJ_TEST("StaticFileServer: serve index.html for root") {
  test::TestContext ctx;
  auto& headerTable = ctx.headerTable();

  TempDir tempDir;
  tempDir.writeFile("index.html", "<html><body>Root Index</body></html>"_kj);

  StaticFileServer::Config config;
  config.staticDir = tempDir.nativePath();
  StaticFileServer server(config);

  kj::HttpHeaders headers(headerTable);

  // Root path should serve index.html
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 200);
    KJ_IF_SOME(ct, response.responseHeaders.get(kj::HttpHeaderId::CONTENT_TYPE)) {
      KJ_EXPECT(ct == "text/html; charset=utf-8");
    }
  }

  // Empty path should also serve index.html
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 200);
  }
}

// Test cache headers
KJ_TEST("StaticFileServer: cache headers") {
  test::TestContext ctx;
  auto& headerTable = ctx.headerTable();

  TempDir tempDir;
  tempDir.writeFile("cached.js", "console.log('cached')"_kj);

  StaticFileServer::Config config;
  config.staticDir = tempDir.nativePath();
  config.enableCache = true;
  config.maxAge = 3600;
  StaticFileServer server(config);

  kj::HttpHeaders headers(headerTable);
  test::MockHttpResponse response(headerTable);

  server.serve_file(kj::HttpMethod::GET, "/cached.js", headers, response).wait(ctx.waitScope());
  KJ_EXPECT(response.statusCode == 200);

  // Check Cache-Control header
  auto cc = find_header(response.responseHeaders, "Cache-Control"_kj);
  KJ_IF_SOME(cacheControl, cc) {
    KJ_EXPECT(cacheControl == "public, max-age=3600");
  }
  else {
    KJ_FAIL_ASSERT("Expected Cache-Control header");
  }

  // Check ETag header
  auto etag = find_header(response.responseHeaders, "ETag"_kj);
  KJ_IF_SOME(e, etag) {
    KJ_EXPECT(e.startsWith("\""));
    KJ_EXPECT(e.endsWith("\""));
  }
  else {
    KJ_FAIL_ASSERT("Expected ETag header");
  }

  // Check Last-Modified header
  auto lm = find_header(response.responseHeaders, "Last-Modified"_kj);
  KJ_IF_SOME(lastModified, lm) {
    KJ_EXPECT(lastModified.endsWith(" GMT"));
  }
  else {
    KJ_FAIL_ASSERT("Expected Last-Modified header");
  }
}

// Test cache disabled
KJ_TEST("StaticFileServer: cache disabled") {
  test::TestContext ctx;
  auto& headerTable = ctx.headerTable();

  TempDir tempDir;
  tempDir.writeFile("uncached.js", "console.log('uncached')"_kj);

  StaticFileServer::Config config;
  config.staticDir = tempDir.nativePath();
  config.enableCache = false;
  StaticFileServer server(config);

  kj::HttpHeaders headers(headerTable);
  test::MockHttpResponse response(headerTable);

  server.serve_file(kj::HttpMethod::GET, "/uncached.js", headers, response).wait(ctx.waitScope());
  KJ_EXPECT(response.statusCode == 200);

  // Cache headers should not be present
  auto cc = find_header(response.responseHeaders, "Cache-Control"_kj);
  KJ_EXPECT(cc == kj::none);

  auto etag = find_header(response.responseHeaders, "ETag"_kj);
  KJ_EXPECT(etag == kj::none);
}

// Test method restriction
KJ_TEST("StaticFileServer: only GET and HEAD allowed") {
  test::TestContext ctx;
  auto& headerTable = ctx.headerTable();

  TempDir tempDir;
  tempDir.writeFile("file.txt", "content"_kj);

  StaticFileServer::Config config;
  config.staticDir = tempDir.nativePath();
  StaticFileServer server(config);

  kj::HttpHeaders headers(headerTable);

  // POST should be rejected
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::POST, "/file.txt", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 405); // Method Not Allowed
  }

  // PUT should be rejected
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::PUT, "/file.txt", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 405);
  }

  // DELETE should be rejected
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::DELETE, "/file.txt", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 405);
  }

  // GET should work
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/file.txt", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 200);
  }

  // HEAD should work
  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::HEAD, "/file.txt", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 200);
  }
}

// Test 404 for non-existent files
KJ_TEST("StaticFileServer: 404 for non-existent files") {
  test::TestContext ctx;
  auto& headerTable = ctx.headerTable();

  TempDir tempDir;
  // Don't create index.html - test 404

  StaticFileServer::Config config;
  config.staticDir = tempDir.nativePath();
  StaticFileServer server(config);

  kj::HttpHeaders headers(headerTable);

  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/nonexistent.html", headers, response)
        .wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 404);
  }

  {
    test::MockHttpResponse response(headerTable);
    server.serve_file(kj::HttpMethod::GET, "/missing.js", headers, response).wait(ctx.waitScope());
    KJ_EXPECT(response.statusCode == 404);
  }
}

// Test conditional requests with If-None-Match (ETag)
KJ_TEST("StaticFileServer: conditional request with ETag") {
  test::TestContext ctx;
  auto& headerTable = ctx.headerTable();

  TempDir tempDir;
  tempDir.writeFile("conditional.js", "console.log('conditional')"_kj);

  StaticFileServer::Config config;
  config.staticDir = tempDir.nativePath();
  config.enableCache = true;
  StaticFileServer server(config);

  // First request to get ETag
  kj::HttpHeaders headers1(headerTable);
  test::MockHttpResponse response1{headerTable};
  server.serve_file(kj::HttpMethod::GET, "/conditional.js", headers1, response1)
      .wait(ctx.waitScope());
  KJ_EXPECT(response1.statusCode == 200);

  auto etag = find_header(response1.responseHeaders, "ETag"_kj);
  KJ_IF_SOME(e, etag) {
    // Second request with If-None-Match
    kj::HttpHeaders headers2(headerTable);
    headers2.addPtrPtr("If-None-Match"_kj, e);
    test::MockHttpResponse response2{headerTable};
    server.serve_file(kj::HttpMethod::GET, "/conditional.js", headers2, response2)
        .wait(ctx.waitScope());
    KJ_EXPECT(response2.statusCode == 304); // Not Modified
  }
  else {
    KJ_FAIL_ASSERT("Expected ETag header");
  }
}

// Test is_file_path
KJ_TEST("StaticFileServer: is_file_path detection") {
  TempDir tempDir;
  tempDir.writeFile("exists.txt", "content"_kj);
  tempDir.writeFile("index.html", "<html></html>"_kj);

  StaticFileServer::Config config;
  config.staticDir = tempDir.nativePath();
  StaticFileServer server(config);

  // Existing file
  KJ_EXPECT(server.is_file_path("/exists.txt"));

  // Non-existent file
  KJ_EXPECT(!server.is_file_path("/nonexistent.txt"));

  // Root path
  KJ_EXPECT(!server.is_file_path("/"));

  // Empty path
  KJ_EXPECT(!server.is_file_path(""));

  // Directory path
  KJ_EXPECT(!server.is_file_path("/some/dir/"));
}

} // namespace veloz::gateway
