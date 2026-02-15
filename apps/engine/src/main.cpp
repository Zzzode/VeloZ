#include "veloz/engine/engine_app.h"

#include <kj/array.h>
#include <kj/io.h>
#include <kj/main.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <unistd.h>

namespace {

bool has_flag(kj::ArrayPtr<const kj::StringPtr> args, kj::StringPtr flag) {
  for (const auto& a : args) {
    if (a == flag) {
      return true;
    }
  }
  return false;
}

} // namespace

int main(int argc, char** argv) {
  kj::Vector<kj::StringPtr> args;
  args.reserve(static_cast<size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    args.add(argv[i] ? kj::StringPtr(argv[i]) : ""_kj);
  }

  veloz::engine::EngineConfig config;
  config.stdio_mode = has_flag(args.asPtr(), "--stdio"_kj);

  kj::FdOutputStream stdoutStream(STDOUT_FILENO);
  kj::FdOutputStream stderrStream(STDERR_FILENO);
  veloz::engine::EngineApp app(config, stdoutStream, stderrStream);
  return app.run();
}
