#include "veloz/core/event_loop.h"
#include "veloz/engine/engine_app.h"
#include "veloz/engine/event_emitter.h"
#include "veloz/engine/market_data_manager.h"
#include "veloz/market/market_event.h"

#include <atomic>
#include <chrono>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/string.h>
#include <kj/test.h>
#include <kj/thread.h>
#include <kj/vector.h>
#include <thread>

using namespace veloz::engine;

namespace veloz::engine {
struct EngineAppTestAccess {
  static void start_event_loop(EngineApp& app) {
    app.start_event_loop();
  }

  static void stop_event_loop(EngineApp& app) {
    app.stop_event_loop();
  }

  static bool is_event_loop_running(const EngineApp& app) {
    return app.is_event_loop_running();
  }
};

struct MarketDataManagerTestAccess {
  static kj::Vector<veloz::core::EventTag> tags(const MarketDataManager& manager,
                                                const veloz::market::MarketEvent& event) {
    return manager.build_market_event_tags(event);
  }

  static veloz::core::EventPriority priority(const MarketDataManager& manager,
                                             const veloz::market::MarketEvent& event) {
    return manager.market_event_priority(event);
  }
};
} // namespace veloz::engine

namespace {

class VectorOutputStream : public kj::OutputStream {
public:
  void write(kj::ArrayPtr<const kj::byte> buffer) override {
    for (auto b : buffer) {
      data_.add(static_cast<char>(b));
    }
  }

  kj::String getString() const {
    kj::Vector<char> copy;
    for (auto c : data_) {
      copy.add(c);
    }
    copy.add('\0');
    return kj::String(copy.releaseAsArray());
  }

private:
  kj::Vector<char> data_;
};

KJ_TEST("EngineApp: EventLoop lifecycle") {
  EngineConfig config;
  VectorOutputStream out;
  VectorOutputStream err;
  EngineApp app(config, out, err);

  EngineAppTestAccess::start_event_loop(app);

  for (int i = 0; i < 200 && !EngineAppTestAccess::is_event_loop_running(app); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  KJ_EXPECT(EngineAppTestAccess::is_event_loop_running(app));
  EngineAppTestAccess::stop_event_loop(app);
  KJ_EXPECT(!EngineAppTestAccess::is_event_loop_running(app));
}

KJ_TEST("MarketDataManager: Tag policy") {
  auto io = kj::setupAsyncIo();
  VectorOutputStream out;
  EventEmitter emitter(out);
  MarketDataManager::Config config;
  MarketDataManager manager(io, emitter, config);

  veloz::market::MarketEvent event;
  event.type = veloz::market::MarketEventType::Kline;
  event.venue = veloz::common::Venue::Binance;
  event.market = veloz::common::MarketKind::Spot;
  event.symbol = veloz::common::SymbolId("BTCUSDT"_kj);

  auto tags = MarketDataManagerTestAccess::tags(manager, event);
  bool has_market = false;
  bool has_type = false;
  bool has_venue = false;
  bool has_symbol = false;
  for (auto& tag : tags) {
    if (tag == "market"_kj)
      has_market = true;
    if (tag == "type:kline"_kj)
      has_type = true;
    if (tag == "venue:Binance"_kj)
      has_venue = true;
    if (tag == "symbol:BTCUSDT"_kj)
      has_symbol = true;
  }

  KJ_EXPECT(has_market);
  KJ_EXPECT(has_type);
  KJ_EXPECT(has_venue);
  KJ_EXPECT(has_symbol);
  KJ_EXPECT(MarketDataManagerTestAccess::priority(manager, event) ==
            veloz::core::EventPriority::Low);
}

KJ_TEST("MarketDataManager: EventLoop tag filter") {
  auto io = kj::setupAsyncIo();
  VectorOutputStream out;
  EventEmitter emitter(out);
  MarketDataManager::Config config;
  auto loop = kj::heap<veloz::core::EventLoop>();
  MarketDataManager manager(io, emitter, config, *loop);

  veloz::market::MarketEvent event;
  event.type = veloz::market::MarketEventType::Kline;
  event.venue = veloz::common::Venue::Binance;
  event.market = veloz::common::MarketKind::Spot;
  event.symbol = veloz::common::SymbolId("BTCUSDT"_kj);

  auto tags = MarketDataManagerTestAccess::tags(manager, event);
  std::atomic<int> executed{0};
  (void)loop->add_tag_filter("type:kline"_kj);
  loop->post_with_tags([&] { ++executed; }, veloz::core::EventPriority::Normal, kj::mv(tags));

  veloz::core::EventLoop* loop_ptr = loop.get();
  {
    kj::Thread worker([loop_ptr] { loop_ptr->run(); });
    while (!loop->is_running()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    loop->stop();
  }

  KJ_EXPECT(executed.load() == 0);
}

} // namespace
