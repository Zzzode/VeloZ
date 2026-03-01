#pragma once

#include <chrono>
#include <kj/async.h>
#include <kj/compat/http.h>
#include <kj/filesystem.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

/**
 * Static file server for serving web UI assets.
 *
 * Features:
 * - MIME type detection for common web file types
 * - Security: prevents path traversal attacks
 * - Cache headers: Cache-Control, ETag, Last-Modified
 * - SPA routing: serves index.html for non-file paths
 * - Directory index: serves index.html for directory requests
 */
class StaticFileServer {
public:
  /**
   * Configuration for the static file server.
   */
  struct Config {
    kj::String staticDir;                    // Directory containing static files
    bool enableCache = true;                 // Enable cache headers
    uint32_t maxAge = 3600;                  // Cache max-age in seconds (default: 1 hour)
    uint64_t maxFileSize = 10 * 1024 * 1024; // Max file size: 10MB
  };

  /**
   * File information structure.
   */
  struct FileInfo {
    kj::Array<kj::byte> content;
    kj::String contentType;
    uint64_t size;
    kj::String etag;
    kj::String lastModified;
  };

  /**
   * Construct a static file server.
   *
   * @param config Server configuration
   */
  explicit StaticFileServer(const Config& config);

  /**
   * Serve a file from the static directory.
   *
   * @param method HTTP method
   * @param path Path relative to static directory (e.g., "/index.html", "/assets/main.js")
   * @param headers Request headers
   * @param response HTTP response object
   * @return Promise that completes when the file is sent or error returned
   */
  kj::Promise<void> serve_file(kj::HttpMethod method, kj::StringPtr path,
                               const kj::HttpHeaders& headers, kj::HttpService::Response& response);

  /**
   * Check if a path refers to a static file (not a directory).
   *
   * @param path Path to check
   * @return true if the path is a file, false otherwise
   */
  bool is_file_path(kj::StringPtr path) const;

  /**
   * Get the static directory path.
   */
  kj::StringPtr static_dir() const {
    return config_.staticDir;
  }

private:
  Config config_;
  kj::Own<kj::Filesystem> fs_;
  kj::Own<const kj::ReadableDirectory> rootDir_;

  /**
   * Read a file from the filesystem.
   *
   * @param path Path relative to static directory
   * @return FileInfo if the file exists and is readable, nullptr otherwise
   */
  kj::Maybe<FileInfo> read_file(kj::StringPtr path);

  /**
   * Detect content type based on file extension.
   *
   * @param path File path with extension
   * @return MIME type string
   */
  kj::String detect_content_type(kj::StringPtr path) const;

  /**
   * Check if a path is safe (no path traversal).
   *
   * @param path Path to check
   * @return true if the path is safe, false if it attempts path traversal
   */
  bool is_safe_path(kj::StringPtr path) const;

  /**
   * Generate ETag for a file.
   *
   * @param content File content
   * @param lastModified Last modification time
   * @return ETag string
   */
  kj::String generate_etag(kj::ArrayPtr<const kj::byte> content,
                           std::chrono::system_clock::time_point lastModified) const;

  /**
   * Format time for HTTP headers (RFC 7231 format).
   *
   * @param time Time point to format
   * @return Formatted time string
   */
  kj::String format_http_time(std::chrono::system_clock::time_point time) const;

  /**
   * Send file response with cache headers.
   *
   * @param info File information
   * @param requestHeaders Request headers (for conditional requests)
   * @param response HTTP response object
   * @return Promise that completes when the response is sent
   */
  kj::Promise<void> send_file_response(FileInfo info, const kj::HttpHeaders& requestHeaders,
                                       kj::HttpService::Response& response);
};

} // namespace veloz::gateway
