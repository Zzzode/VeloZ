#include "veloz/engine/engine_app.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

bool has_flag(const std::vector<std::string>& args, std::string_view flag) {
  for (const auto& a : args) {
    if (a == flag) {
      return true;
    }
  }
  return false;
}

} // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i] ? argv[i] : "");
  }

  veloz::engine::EngineConfig config;
  config.stdio_mode = has_flag(args, "--stdio");
  veloz::engine::EngineApp app(config, std::cout, std::cerr);
  return app.run();
}
