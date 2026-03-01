#include "static/static_file_server.h"

#include <ctime>
#include <iomanip>
#include <kj/async-io.h>
#include <kj/debug.h>
#include <kj/time.h>
#include <sstream>

namespace veloz::gateway {

// ============================================================================
// MIME Type Registry
// ============================================================================

namespace {

const kj::HashMap<kj::StringPtr, kj::StringPtr>& get_mime_type_map() {
  static const kj::HashMap<kj::StringPtr, kj::StringPtr> mime_types = [] {
    kj::HashMap<kj::StringPtr, kj::StringPtr> map;
    map.insert(".html", "text/html; charset=utf-8"_kj);
    map.insert(".htm", "text/html; charset=utf-8"_kj);
    map.insert(".css", "text/css; charset=utf-8"_kj);
    map.insert(".js", "application/javascript; charset=utf-8"_kj);
    map.insert(".mjs", "application/javascript; charset=utf-8"_kj);
    map.insert(".json", "application/json; charset=utf-8"_kj);
    map.insert(".xml", "application/xml; charset=utf-8"_kj);
    map.insert(".png", "image/png"_kj);
    map.insert(".jpg", "image/jpeg"_kj);
    map.insert(".jpeg", "image/jpeg"_kj);
    map.insert(".gif", "image/gif"_kj);
    map.insert(".svg", "image/svg+xml"_kj);
    map.insert(".ico", "image/x-icon"_kj);
    map.insert(".webp", "image/webp"_kj);
    map.insert(".woff", "font/woff"_kj);
    map.insert(".woff2", "font/woff2"_kj);
    map.insert(".ttf", "font/ttf"_kj);
    map.insert(".otf", "font/otf"_kj);
    map.insert(".eot", "application/vnd.ms-fontobject"_kj);
    map.insert(".wasm", "application/wasm"_kj);
    map.insert(".pdf", "application/pdf"_kj);
    map.insert(".zip", "application/zip"_kj);
    map.insert(".gz", "application/gzip"_kj);
    map.insert(".tar", "application/x-tar"_kj);
    map.insert(".txt", "text/plain; charset=utf-8"_kj);
    map.insert(".md", "text/markdown; charset=utf-8"_kj);
    map.insert(".csv", "text/csv; charset=utf-8"_kj);
    return map;
  }();
  return mime_types;
}

kj::Own<const kj::ReadableDirectory> open_static_root(kj::Filesystem& fs, kj::StringPtr staticDir) {
  auto path = fs.getCurrentPath().evalNative(staticDir);
  KJ_REQUIRE(fs.getRoot().exists(path), "Static directory does not exist", staticDir);
  return fs.getRoot().openSubdir(kj::mv(path));
}

// Get file extension from path
kj::String get_extension(kj::StringPtr path) {
  // Find last dot after the last slash
  KJ_IF_SOME(lastSlash, path.findLast('/')) {
    KJ_IF_SOME(lastDot, path.slice(lastSlash + 1).findLast('.')) {
      if (lastDot == 0) {
        // Hidden file like .gitignore
        return kj::str();
      }
      return kj::str(path.slice(lastSlash + 1 + lastDot));
    }
  }
  else {
    KJ_IF_SOME(lastDot, path.findLast('.')) {
      if (lastDot == 0) {
        return kj::str();
      }
      return kj::str(path.slice(lastDot));
    }
  }
  return kj::str();
}

// Helper function to get HTTP header
kj::Maybe<kj::StringPtr> get_http_header(const kj::HttpHeaders& headers, kj::StringPtr name) {
  kj::Maybe<kj::StringPtr> result = kj::none;
  headers.forEach([&](kj::StringPtr headerName, kj::StringPtr headerValue) {
    if (headerName == name) {
      result = headerValue;
    }
  });
  return result;
}

} // namespace

// ============================================================================
// StaticFileServer Implementation
// ============================================================================

StaticFileServer::StaticFileServer(const Config& config)
    : config_{kj::heapString(config.staticDir), config.enableCache, config.maxAge,
              config.maxFileSize},
      fs_(kj::newDiskFilesystem()), rootDir_(open_static_root(*fs_, config_.staticDir)) {}

kj::Promise<void> StaticFileServer::serve_file(kj::HttpMethod method, kj::StringPtr path,
                                               const kj::HttpHeaders& headers,
                                               kj::HttpService::Response& response) {

  try {
    // Only handle GET and HEAD requests
    if (method != kj::HttpMethod::GET && method != kj::HttpMethod::HEAD) {
      return response.sendError(405, "Method Not Allowed"_kj, headers);
    }

    // Security check: prevent path traversal
    if (!is_safe_path(path)) {
      KJ_LOG(WARNING, "Path traversal attempt blocked", path);
      return response.sendError(403, "Forbidden"_kj, headers);
    }

    // Normalize path: remove leading slash
    kj::StringPtr normalizedPath = path;
    if (normalizedPath.startsWith("/")) {
      normalizedPath = normalizedPath.slice(1);
    }

    // Handle empty path or directory index
    if (normalizedPath.size() == 0 || normalizedPath == "/"_kj) {
      // Serve index.html
      KJ_IF_SOME(info, read_file("index.html")) {
        return send_file_response(kj::mv(info), headers, response);
      }
      return response.sendError(404, "Not Found"_kj, headers);
    }

    // Check if path is a directory (ends with /)
    if (normalizedPath.endsWith("/")) {
      // Try to serve index.html from directory
      kj::String indexPath = kj::str(normalizedPath, "index.html");
      KJ_IF_SOME(info, read_file(indexPath)) {
        return send_file_response(kj::mv(info), headers, response);
      }
      // Directory without index.html
      return response.sendError(404, "Not Found"_kj, headers);
    }

    // Try to read the file
    KJ_IF_SOME(info, read_file(normalizedPath)) {
      return send_file_response(kj::mv(info), headers, response);
    }

    // File not found - serve index.html for SPA routing
    // Only do SPA fallback if the path doesn't look like a file (no extension)
    auto ext = get_extension(normalizedPath);
    if (ext.size() == 0) {
      // This looks like a SPA route, serve index.html
      KJ_IF_SOME(indexInfo, read_file("index.html")) {
        return send_file_response(kj::mv(indexInfo), headers, response);
      }
    }

    return response.sendError(404, "Not Found"_kj, headers);
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error serving static file", path, e);
    return response.sendError(500, "Internal Server Error"_kj, headers);
  }
}

bool StaticFileServer::is_file_path(kj::StringPtr path) const {
  // Normalize path
  kj::StringPtr normalizedPath = path;
  if (normalizedPath.startsWith("/")) {
    normalizedPath = normalizedPath.slice(1);
  }

  // Empty or root path is not a file
  if (normalizedPath.size() == 0 || normalizedPath == "/"_kj) {
    return false;
  }

  // Paths ending with / are directories
  if (normalizedPath.endsWith("/")) {
    return false;
  }

  // Check if the file exists
  try {
    auto relPath = kj::Path::parse(normalizedPath);
    return rootDir_->exists(relPath);
  } catch (...) {
    return false;
  }
}

kj::Maybe<StaticFileServer::FileInfo> StaticFileServer::read_file(kj::StringPtr path) {
  try {
    // Parse relative path
    auto relPath = kj::Path::parse(path);

    // Check if the file exists
    if (!rootDir_->exists(relPath)) {
      return kj::none;
    }

    // Try to open as a file
    KJ_IF_SOME(file, rootDir_->tryOpenFile(relPath)) {
      // Get file stats
      auto stat = file->stat();
      KJ_REQUIRE(stat.size <= config_.maxFileSize, "File too large", path, stat.size,
                 config_.maxFileSize);

      // Read file content using mmap
      auto content = file->mmap(0, stat.size);
      kj::Array<kj::byte> data = kj::heapArray<kj::byte>(content.asBytes());

      auto lastModifiedNs = (stat.lastModified - kj::UNIX_EPOCH) / kj::NANOSECONDS;
      auto lastModifiedDuration = std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::nanoseconds(lastModifiedNs));
      auto lastModified = std::chrono::system_clock::time_point(lastModifiedDuration);

      // Generate ETag
      auto etag = generate_etag(data, lastModified);

      // Format last modified time
      auto lastModifiedStr = format_http_time(lastModified);

      // Detect content type
      auto contentType = detect_content_type(path);

      return FileInfo{kj::mv(data), kj::mv(contentType), stat.size, kj::mv(etag),
                      kj::mv(lastModifiedStr)};
    }
    return kj::none;
  } catch (const kj::Exception& e) {
    KJ_LOG(WARNING, "Failed to read file", path, e);
    return kj::none;
  }
}

kj::String StaticFileServer::detect_content_type(kj::StringPtr path) const {
  auto ext = get_extension(path);
  if (ext.size() == 0) {
    return kj::str("application/octet-stream");
  }

  // Convert to lowercase for lookup
  kj::String extLower = kj::str(ext);
  for (char& c : extLower) {
    if (c >= 'A' && c <= 'Z') {
      c = c - 'A' + 'a';
    }
  }

  const auto& mimeTypes = get_mime_type_map();
  KJ_IF_SOME(mime, mimeTypes.find(extLower)) {
    return kj::str(mime);
  }

  return kj::str("application/octet-stream");
}

bool StaticFileServer::is_safe_path(kj::StringPtr path) const {
  // Check for path traversal attempts
  // 1. ".." segments
  // 2. Null bytes

  if (path.findFirst('\0') != kj::none) {
    return false;
  }

  // Split path into segments
  kj::StringPtr remaining = path;

  while (remaining.size() > 0) {
    KJ_IF_SOME(slashPos, remaining.findFirst('/')) {
      auto segment = remaining.slice(0, slashPos);
      if (segment == ".."_kj) {
        return false;
      }
      remaining = remaining.slice(slashPos + 1);
    }
    else {
      // Last segment
      if (remaining == ".."_kj) {
        return false;
      }
      break;
    }
  }

  return true;
}

kj::String
StaticFileServer::generate_etag(kj::ArrayPtr<const kj::byte> content,
                                std::chrono::system_clock::time_point lastModified) const {
  // Simple ETag: hash of content size and modification time
  auto timestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(lastModified.time_since_epoch())
          .count();

  return kj::str("\"", content.size(), "-", timestamp, "\"");
}

kj::String StaticFileServer::format_http_time(std::chrono::system_clock::time_point time) const {
  // Format according to RFC 7231: "Sun, 06 Nov 1994 08:49:37 GMT"
  auto time_t_val = std::chrono::system_clock::to_time_t(time);
  std::tm* tm_val = std::gmtime(&time_t_val);

  std::ostringstream oss;
  const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  oss << weekdays[tm_val->tm_wday] << ", " << std::setfill('0') << std::setw(2) << tm_val->tm_mday
      << " " << months[tm_val->tm_mon] << " " << (tm_val->tm_year + 1900) << " "
      << std::setfill('0') << std::setw(2) << tm_val->tm_hour << ":" << std::setfill('0')
      << std::setw(2) << tm_val->tm_min << ":" << std::setfill('0') << std::setw(2)
      << tm_val->tm_sec << " GMT";

  return kj::str(oss.str().c_str());
}

kj::Promise<void> StaticFileServer::send_file_response(FileInfo info,
                                                       const kj::HttpHeaders& requestHeaders,
                                                       kj::HttpService::Response& response) {
  auto headers = requestHeaders.clone();
  headers.clear();

  // Set Content-Type and Content-Length using standard headers
  headers.set(kj::HttpHeaderId::CONTENT_TYPE, kj::mv(info.contentType));
  headers.set(kj::HttpHeaderId::CONTENT_LENGTH, kj::str(info.size));

  // Check for conditional request (If-None-Match)
  KJ_IF_SOME(ifNoneMatch, get_http_header(requestHeaders, "If-None-Match")) {
    if (ifNoneMatch == info.etag) {
      // Not modified
      if (config_.enableCache) {
        headers.addPtr("Cache-Control"_kj, kj::str("public, max-age=", config_.maxAge));
        headers.addPtr("ETag"_kj, kj::str(info.etag));
      }
      auto stream = response.send(304, "Not Modified"_kj, headers, kj::none);
      return kj::READY_NOW;
    }
  }

  // Check for conditional request (If-Modified-Since)
  KJ_IF_SOME(ifModifiedSince, get_http_header(requestHeaders, "If-Modified-Since")) {
    if (ifModifiedSince == info.lastModified) {
      // Not modified
      if (config_.enableCache) {
        headers.addPtr("Cache-Control"_kj, kj::str("public, max-age=", config_.maxAge));
        headers.addPtr("Last-Modified"_kj, kj::str(info.lastModified));
      }
      auto stream = response.send(304, "Not Modified"_kj, headers, kj::none);
      return kj::READY_NOW;
    }
  }

  // Add cache headers
  if (config_.enableCache) {
    headers.addPtr("Cache-Control"_kj, kj::str("public, max-age=", config_.maxAge));
    headers.addPtr("ETag"_kj, kj::str(info.etag));
    headers.addPtr("Last-Modified"_kj, kj::str(info.lastModified));
  }

  // Send the file
  auto stream = response.send(200, "OK"_kj, headers, info.size);
  auto writePromise = stream->write(info.content.asBytes());

  return writePromise.attach(kj::mv(stream), kj::mv(info.content));
}

} // namespace veloz::gateway
