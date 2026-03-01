#include "router.h"

#include <kj/compat/http.h>
#include <kj/debug.h>
#include <kj/string.h>

namespace veloz::gateway {

void Router::add_route(kj::HttpMethod method, kj::StringPtr pattern, Handler handler) {
  // Validate pattern format
  KJ_REQUIRE(pattern.startsWith("/"), "Pattern must start with '/'");
  KJ_REQUIRE(!pattern.endsWith("/"_kj) || pattern == "/"_kj,
             "Pattern must not end with '/' (except for root)");

  Route route;
  route.method = method;
  route.pattern = kj::str(pattern);
  route.segments = parse_pattern(pattern);
  route.handler = kj::mv(handler);

  // Add to routes vector
  routes_.add(kj::mv(route));
}

kj::Maybe<Router::RouteMatch> Router::match(kj::HttpMethod method, kj::StringPtr path) {
  // Normalize the input path
  kj::String normalized_path = normalize_path(path);

  // Iterate through routes to find a match
  for (auto& route : routes_) {
    if (route.method != method) {
      continue;
    }

    kj::HashMap<kj::String, kj::String> path_params;
    if (match_pattern(route.segments, normalized_path, path_params)) {
      RouteMatch match;
      // Clone the handler by wrapping it in a lambda that calls the original
      // Since kj::Function is not copyable, we need to store handlers in a way
      // that allows multiple invocations. We use a pointer wrapper approach.
      match.handler = [&handler = route.handler](RequestContext& ctx) -> kj::Promise<void> {
        return handler(ctx);
      };
      match.path_params = kj::mv(path_params);
      return match;
    }
  }

  return kj::none;
}

bool Router::has_path(kj::StringPtr path) const {
  kj::String normalized_path = normalize_path(path);

  for (const auto& route : routes_) {
    if (route.pattern == normalized_path) {
      return true;
    }
  }

  // Check parameterized patterns
  for (const auto& route : routes_) {
    kj::HashMap<kj::String, kj::String> dummy_params;
    if (match_pattern(route.segments, normalized_path, dummy_params)) {
      return true;
    }
  }

  return false;
}

kj::Vector<kj::String> Router::get_methods_for_path(kj::StringPtr path) const {
  kj::Vector<kj::String> methods;
  kj::String normalized_path = normalize_path(path);

  // Use simple approach - track seen methods via Vector lookups
  for (const auto& route : routes_) {
    kj::HashMap<kj::String, kj::String> dummy_params;
    if (route.pattern == normalized_path ||
        match_pattern(route.segments, normalized_path, dummy_params)) {
      // Check if we've already added this method
      bool found = false;
      for (const auto& existing_method : methods) {
        if (existing_method == get_method_name(route.method)) {
          found = true;
          break;
        }
      }
      if (!found) {
        methods.add(get_method_name(route.method));
      }
    }
  }

  return methods;
}

kj::Vector<Router::Route::Segment> Router::parse_pattern(kj::StringPtr pattern) {
  kj::Vector<Route::Segment> segments;

  // Skip leading slash
  kj::StringPtr remaining = pattern.slice(1);
  if (remaining.size() == 0) {
    // Root path "/"
    return segments;
  }

  while (remaining.size() > 0) {
    KJ_IF_SOME(slash_pos, remaining.findFirst('/')) {
      auto segment_str = remaining.slice(0, slash_pos);
      remaining = remaining.slice(slash_pos + 1);

      Route::Segment segment;
      // Check if this is a parameter: {param_name}
      if (segment_str.size() >= 2 && segment_str[0] == '{' &&
          segment_str[segment_str.size() - 1] == '}') {
        // Extract parameter name
        kj::String param_name = kj::str(segment_str.slice(1, segment_str.size() - 1));
        KJ_REQUIRE(param_name.size() > 0, "Parameter name cannot be empty");

        segment.is_param = true;
        segment.value = kj::mv(param_name);
      } else {
        segment.is_param = false;
        segment.value = kj::str(segment_str);
      }

      segments.add(kj::mv(segment));
    }
    else {
      // No more slashes - this is the last segment
      Route::Segment segment;
      // Check if this is a parameter: {param_name}
      if (remaining.size() >= 2 && remaining[0] == '{' && remaining[remaining.size() - 1] == '}') {
        // Extract parameter name
        kj::String param_name = kj::str(remaining.slice(1, remaining.size() - 1));
        KJ_REQUIRE(param_name.size() > 0, "Parameter name cannot be empty");

        segment.is_param = true;
        segment.value = kj::mv(param_name);
      } else {
        segment.is_param = false;
        segment.value = kj::str(remaining);
      }

      segments.add(kj::mv(segment));
      break;
    }
  }

  return segments;
}

bool Router::match_pattern(const kj::Vector<Route::Segment>& pattern_segments, kj::StringPtr path,
                           kj::HashMap<kj::String, kj::String>& path_params) const {
  // Skip leading slash from path
  kj::StringPtr remaining = path.slice(1);

  size_t segment_index = 0;
  while (remaining.size() > 0 && segment_index < pattern_segments.size()) {
    KJ_IF_SOME(slash_pos, remaining.findFirst('/')) {
      auto path_segment = remaining.slice(0, slash_pos);
      remaining = remaining.slice(slash_pos + 1);

      const auto& pattern_segment = pattern_segments[segment_index++];

      if (pattern_segment.is_param) {
        // Extract parameter value
        kj::String param_value = kj::str(path_segment);
        path_params.insert(kj::str(pattern_segment.value), kj::mv(param_value));
      } else {
        // Match literal segment
        if (path_segment != pattern_segment.value) {
          return false;
        }
      }
    }
    else {
      // No more slashes - this is the last path segment
      const auto& pattern_segment = pattern_segments[segment_index++];

      if (pattern_segment.is_param) {
        // Extract parameter value
        kj::String param_value = kj::str(remaining);
        path_params.insert(kj::str(pattern_segment.value), kj::mv(param_value));
      } else {
        // Match literal segment
        if (remaining != pattern_segment.value) {
          return false;
        }
      }

      // Clear remaining since we've processed the last segment
      remaining = kj::StringPtr();
    }
  }

  // Check if all pattern segments were matched and no extra path segments remain
  return segment_index == pattern_segments.size() && remaining.size() == 0;
}

kj::String Router::normalize_path(kj::StringPtr path) {
  KJ_REQUIRE(path.size() > 0, "Path cannot be empty");

  // Ensure path starts with /
  if (!path.startsWith("/")) {
    return kj::str("/", path);
  }

  // Remove trailing slash (except for root)
  if (path.size() > 1 && path.endsWith("/"_kj)) {
    return kj::str(path.slice(0, path.size() - 1));
  }

  return kj::str(path);
}

kj::String Router::get_method_name(kj::HttpMethod method) {
  switch (method) {
  case kj::HttpMethod::GET:
    return kj::str("GET");
  case kj::HttpMethod::POST:
    return kj::str("POST");
  case kj::HttpMethod::PUT:
    return kj::str("PUT");
  case kj::HttpMethod::DELETE:
    return kj::str("DELETE");
  case kj::HttpMethod::PATCH:
    return kj::str("PATCH");
  case kj::HttpMethod::HEAD:
    return kj::str("HEAD");
  case kj::HttpMethod::OPTIONS:
    return kj::str("OPTIONS");
  default:
    return kj::str("UNKNOWN");
  }
}

} // namespace veloz::gateway
